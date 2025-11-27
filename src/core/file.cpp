#include "file.h"
#include "constants.h"

#include "pool.hpp"
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <chrono>
#include <algorithm>

#include "renderer.h"

inline double compute_transfer_speed(file_size_t bytes_transferred, const std::chrono::duration<double, std::nano>& duration) {
    if (duration.count() == 0.0)
        return 0.0;

    double seconds = duration.count() * 1e-9;
    return static_cast<double>(bytes_transferred) / seconds;
}

void buffer_write(
    const std::string &src_path,
    const std::string &dest_path,
    segment_status &status
) {
    std::ifstream src(src_path, std::ios::binary);
    std::ofstream dest(dest_path, std::ios::binary);

    file_size_t ptr = status.get_copied_offset();
    src.seekg(ptr);
    dest.seekp(ptr);
    std::array<char, MIN_BUF_SIZE> buffer;

    while (ptr <= status.end_byte) {
        const file_size_t read_size = std::min<file_size_t>(MIN_BUF_SIZE, (status.end_byte) - ptr);

        // Reading
        auto rt1 = std::chrono::steady_clock::now();
        src.read(buffer.data(), read_size);
        const std::streamsize bytes_read = src.gcount();
        auto rt2 = std::chrono::steady_clock::now();
        if (bytes_read <= 0) break;
        ptr += bytes_read;
        const auto r_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(rt2 - rt1);
        const double r_speed = compute_transfer_speed(bytes_read, r_duration);

        // Writing
        auto wt1 = std::chrono::steady_clock::now();
        dest.write(buffer.data(), bytes_read);
        dest.flush();
        auto wt2 = std::chrono::steady_clock::now();

        const auto w_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(wt2 - wt1);
        const double w_speed = compute_transfer_speed(bytes_read, w_duration);
    }
}

inline file_size_t get_optimal_buffer_size(const double current_throughput, const double prev_throughput, const double duration_ns, const file_size_t current_buffer_size = BASELINE_BUF_SIZE) {
    if (duration_ns <= 0 || current_throughput <= 0) {
        return BASELINE_BUF_SIZE;
    }

    auto buffer_size = current_buffer_size;
    const auto throughput_ratio = prev_throughput > 0 ?
        current_throughput / prev_throughput : 1.0;

    if (duration_ns < HIGH_LATENCY_NS && throughput_ratio > 1.05) {
        buffer_size = static_cast<file_size_t>(std::min<double>(buffer_size * 1.5, MAX_BUF_SIZE));
    } else if (duration_ns > HIGH_LATENCY_NS || throughput_ratio < 0.95) {
        buffer_size = static_cast<file_size_t>(std::max<double>(buffer_size / 1.5, MIN_BUF_SIZE));
    } else {
        buffer_size = static_cast<file_size_t>(std::clamp<double>(buffer_size * 1.05, MIN_BUF_SIZE, MAX_BUF_SIZE));
    }

    return buffer_size;
}

void direct_write(const int &src_fd, const int &dest_fd, const std::string& segment_id, std::shared_ptr<file_transfer_status>& status,
                  console_renderer &renderer, const std::string& root_id) {
    const auto segment = status->get_segment(segment_id);
    const auto last_copied_byte = static_cast<long>(segment->get_copied_offset() + segment->start_byte);
    loff_t off_in = last_copied_byte;
    loff_t off_out = last_copied_byte;

    double current_throughput = 0.0;
    double prev_throughput = 0.0;
    file_size_t buffer_size = BASELINE_BUF_SIZE;

    off_t remaining = segment->end_byte - last_copied_byte;
    auto total_size = segment->end_byte - segment->start_byte + 1;
    file_size_t bytes_threshold = 0;

    while (remaining > 0) {
        const off_t r_size = std::min<off_t>(buffer_size, remaining);
        const auto t1 = std::chrono::steady_clock::now();
        auto res = copy_file_range(src_fd, &off_in, dest_fd, &off_out, r_size, 0);
        if (res <= 0) {
            // Handle EAGAIN/EINTR/EOF/errors
            break;
        }
        remaining -= res;
        bytes_threshold += res;
        const auto t2 = std::chrono::steady_clock::now();
        const auto r_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1);

        prev_throughput = current_throughput;
        current_throughput = compute_transfer_speed(res, r_duration);

        if (bytes_threshold >= UPDATE_THRESHOLD) {
            status->add_copied_byte(segment_id, bytes_threshold);
            renderer.enqueue(
                r_frame(segment_id, total_size - remaining, total_size, current_throughput, current_throughput));

            renderer.enqueue(
                r_frame(root_id, status->get_total_copied(), status->total_size, current_throughput, current_throughput));
            bytes_threshold = 0;
        }

        buffer_size = get_optimal_buffer_size(current_throughput, prev_throughput, r_duration.count(), buffer_size);
    }
}

void write_segments(std::shared_ptr<file_transfer_status> &status) {
    const std::string source_file = status->source_file;
    const std::string dest_file = status->destination_file;
    thread_pool pool(status->segments.size());
    const int src_fd = open(source_file.c_str(), O_RDONLY);
    if (src_fd < 0) {
        throw std::runtime_error("failed to open source file");
    }
    const int dest_fd = open(dest_file.c_str(), O_WRONLY);
    if (dest_fd < 0) {
        throw std::runtime_error("failed to open destination file");
    }

    console_renderer renderer;
    renderer.start();
    const std::string root_id = generate_uuid();
    renderer.allocate_space(root_id);

    for (const auto &segment_id: status->segments | std::views::keys) {
        renderer.allocate_space(segment_id);

        pool.submit([&, segment_id] mutable {
            direct_write(src_fd, dest_fd, segment_id, status, renderer, root_id);
        });
    }
    pool.stop();
    renderer.stop();
}

std::pair<file_size_t, file_size_t> get_segment_size(const file_size_t &file_size, const int &thread_count) {
    if (file_size <= MIN_FILE_SEGMENTS) {
        return std::make_pair(file_size, 0);
    }
    file_size_t segment_size = file_size / thread_count;
    const auto remainder = file_size % thread_count;
    return std::make_pair(segment_size, remainder);
}
