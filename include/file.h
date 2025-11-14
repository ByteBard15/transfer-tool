#ifndef FILE_H
#define FILE_H
#include <fstream>
#include <string>

#include "types.h"
#include "json.hpp"
#include "parser.h"
#include "logger.h"
#include <openssl/evp.h>

#include "constants.h"

struct segment_status {
    std::string segment_id;
    const file_size_t start_byte;
    const file_size_t end_byte;

private:
    std::atomic<file_size_t> last_copied_;
    std::atomic<bool> completed_;

public:
    segment_status(const std::string &segment_id, const file_size_t &start,
                   const file_size_t &end): segment_id(segment_id), start_byte(start),
                                            end_byte(end), last_copied_(0), completed_(false) {
    }

    void add_copied(const file_size_t &delta) noexcept {
        last_copied_.fetch_add(delta, std::memory_order_relaxed);
    }

    void set_last_copied(file_size_t val) noexcept {
        auto old_val = last_copied_.load(std::memory_order_relaxed);
        while (!last_copied_.compare_exchange_weak(old_val, val, std::memory_order_relaxed)) {
        }
    }

    file_size_t get_last_copied() const noexcept {
        return last_copied_.load(std::memory_order_relaxed);
    }

    void set_completed(bool completed) noexcept {
        completed_.store(completed, std::memory_order_release);
    }

    bool get_completed() const noexcept {
        return completed_.load(std::memory_order_acquire);
    }

    nlohmann::json to_json() const {
        return {
            {"segment_id", segment_id},
            {"start_byte", start_byte},
            {"end_byte", end_byte},
            {"last_copied_byte", last_copied_.load(std::memory_order_relaxed)},
            {"completed", completed_.load(std::memory_order_relaxed)}
        };
    }

    static segment_status from_json(const nlohmann::json &j) {
        const auto segment_id = j.value("segment_id", "");
        const auto start_byte = j.value("start_byte", 0ULL);
        const auto end_byte = j.value("end_byte", 0ULL);
        const auto last_copied_byte = j.value("last_copied_byte", 0ULL);
        const auto completed = j.value("completed", false);

        segment_status seg(segment_id, start_byte, end_byte);
        seg.set_last_copied(last_copied_byte);
        seg.set_completed(completed);
        return seg;
    }

    segment_status(const segment_status &s_status): segment_id(s_status.segment_id), start_byte(s_status.start_byte),
                                                    end_byte(s_status.end_byte),
                                                    last_copied_(s_status.get_last_copied()),
                                                    completed_(s_status.get_completed()) {
    }
};

struct file_transfer_status {
    const std::string source_file;
    const std::string destination_file;
    const file_size_t total_size;
    std::atomic<file_size_t> total_copied_;
    const std::string checksum;
    std::vector<std::shared_ptr<segment_status>> segments;

    file_transfer_status()
        : total_size(0), total_copied_(0) {
    }

    file_transfer_status(const std::string &source_file, const std::string &destination_file,
                         const file_size_t &total_size, const file_size_t &total_copied,
                         const std::string &checksum): source_file(source_file),
                                                       destination_file(destination_file), total_size(total_size),
                                                       total_copied_(total_copied), checksum(checksum) {
    }

    file_size_t get_total_copied() const noexcept {
        return total_copied_.load(std::memory_order_relaxed);
    }

    void add_copied_bytes(const file_size_t& copied) {
        total_copied_.fetch_add(copied, std::memory_order_relaxed);
    }

    std::shared_ptr<segment_status> get_segment(segment_id)

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["source_file"] = source_file;
        j["destination_file"] = destination_file;
        j["total_size"] = total_size;
        j["total_copied"] = total_copied_.load(std::memory_order_relaxed);
        j["checksum"] = checksum;

        j["segments"] = nlohmann::json::array();
        for (const auto &seg: segments) {
            j["segments"].push_back(seg->to_json());
        }

        return j;
    }

    static file_transfer_status from_json(const nlohmann::json &j) {
        const auto source_file = j.value("source_file", "");
        const auto destination_file = j.value("destination_file", "");
        const auto total_size = j.value("total_size", 0ULL);
        const auto total_copied = j.value("total_copied", 0ULL);
        const auto checksum = j.value("checksum", "");

        file_transfer_status ft_status(source_file, destination_file, total_size, total_copied, checksum);

        if (j.contains("segments") && j["segments"].is_array()) {
            for (const auto &item: j["segments"]) {
                ft_status.segments.push_back(std::make_shared<segment_status>(segment_status::from_json(item)));
            }
        }

        return ft_status;
    }

    file_transfer_status(const file_transfer_status &ft_status): source_file(ft_status.source_file),
                                                                 destination_file(ft_status.destination_file),
                                                                 total_size(ft_status.total_size),
                                                                 total_copied_(ft_status.get_total_copied()),
                                                                 checksum(ft_status.checksum) {
        segments.reserve(ft_status.segments.size());

        for (auto segment: ft_status.segments) {
            segments.push_back(std::move(segment));
        }
    }
};

inline std::ostream &operator<<(std::ostream &os, const segment_status &ss) {
    return os << ss.to_json();
}

inline std::ostream &operator<<(std::ostream &os, const file_transfer_status &ts) {
    return os << ts.to_json();
}

inline file_size_t get_checksum_chunk_size(const file_size_t &file_size) {
    if (file_size <= CHECKSUM_LOWER_BOUND) {
        return static_cast<int>(file_size);
    }

    if (file_size >= MAX_CHECKSUM_FILE_SIZE) {
        return CHECKSUM_UPPER_BOUND;
    }

    const file_size_t proportional = file_size / CHECKSUM_SCALE_FACTOR;
    return std::min(proportional, CHECKSUM_UPPER_BOUND);
}

inline std::string compute_checksum(const std::string &path, const file_size_t &chunk_size) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Could not open file " + path);
    }

    std::streamsize file_size = file.tellg();

    if (file_size == chunk_size) {
        std::vector<unsigned char> buffer(file_size);
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char *>(buffer.data()), file_size);

        unsigned char result[EVP_MAX_MD_SIZE];
        unsigned int result_len = 0;

        EVP_MD_CTX *ctx = EVP_MD_CTX_new();
        EVP_DigestInit_ex(ctx, EVP_md5(), nullptr);
        EVP_DigestUpdate(ctx, buffer.data(), buffer.size());
        EVP_DigestFinal_ex(ctx, result, &result_len);
        EVP_MD_CTX_free(ctx);

        std::ostringstream res;
        for (unsigned char i: result)
            res << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(i);
        return res.str();
    }

    std::vector<unsigned char> buffer(2 * chunk_size);

    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char *>(&buffer[0]), static_cast<std::streamsize>(chunk_size));

    file.seekg(static_cast<std::streamsize>(file_size - chunk_size), std::ios::beg);
    file.read(reinterpret_cast<char *>(&buffer[chunk_size]), static_cast<std::streamsize>(chunk_size));

    unsigned char result[EVP_MAX_MD_SIZE];
    unsigned int result_len = 0;

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_md5(), nullptr);
    EVP_DigestUpdate(ctx, buffer.data(), buffer.size());
    EVP_DigestFinal_ex(ctx, result, &result_len);
    EVP_MD_CTX_free(ctx);

    std::ostringstream res;
    for (int i = 0; i < result_len; i++) {
        res << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(result[i]);
    }
    return res.str();
}

file_transfer_status get_transfer_status(const ts_config &config, logger &s_logger, const int& thread_count);

std::pair<file_size_t, file_size_t> get_segment_size(const file_size_t& file_size, const int& thread_count);

inline void prepare_target_file(const std::string &path, const file_size_t &file_size) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        throw std::runtime_error("Failed to open file");
    }

    file.seekp(static_cast<std::streamsize>(file_size) - 1);
    file.write("", 1);
}

void write_segments(const file_transfer_status &status);

#endif
