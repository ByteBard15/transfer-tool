//
// Created by bytebard on 10/26/25.
//

#ifndef RENDERER_H
#define RENDERER_H
#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <queue>
#include <thread>
#include <termios.h>
#include <unordered_map>
#include <sys/ioctl.h>

#include "constants.h"
#include "types.h"

constexpr int DEFAULT_ROW_SIZE = 3;
constexpr int PROGRESS_SIZE = 20;
constexpr char FILLED_PROGRESS = '#';
constexpr char EMPTY_PROGRESS = '-';

std::pair<int, int> get_cursor_position();

inline void move_cursor(int row, int col) {
    std::cout << "\033[" << row << ";" << col << "H";
}

inline void clear_line() {
    std::cout << "\033[2K";
}

inline void clear_line(const int row, const int col) {
    std::cout << "\033[s";
    std::cout << "\033[" << row << ";" << col << "H";
    std::cout << "\033[2K";
    std::cout << "\033[u" << std::flush;
}

inline void hide_cursor() { std::cout << "\033[?25l"; }
inline void show_cursor() { std::cout << "\033[?25h"; }

inline std::pair<int, int> get_terminal_size() {
    struct winsize w{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
        throw std::runtime_error("failed to get terminal size");
    }
    return { w.ws_col, w.ws_row };
}

struct r_frame {
    std::string segment_id;
    file_size_t copied;
    file_size_t total;
    file_size_t r_speed;
    file_size_t w_speed;

    r_frame() = default;
    r_frame(std::string seg, const file_size_t copied, const file_size_t total, const file_size_t r_speed, const file_size_t w_speed)
        : segment_id(std::move(seg)), copied(copied), total(total), r_speed(r_speed), w_speed(w_speed) {}
};

struct view_segment {
    const int t_row;
    const int t_col;

    view_segment(const int row, const int col) : t_row(row), t_col(col) {}
};

class console_renderer {
public:
    console_renderer() {
        auto t_size = get_terminal_size();
        auto c_pos = get_cursor_position();
        t_row_size = t_size.first;
        t_col_size = t_size.second;
        c_start_row = c_pos.first;
        c_start_col = c_pos.second;
        running_.store(false);
    }

    ~console_renderer() {
        stop();
    }

    console_renderer(const console_renderer&) = delete;
    console_renderer& operator=(const console_renderer&) = delete;

    void enqueue(r_frame frame) {
        auto p = std::make_unique<r_frame>(std::move(frame));
        {
            std::lock_guard<std::mutex> lk(m_);
            r_queue.push(std::move(p));
        }
        cv_.notify_one();
    }

    void allocate_space(const std::string& id, int rows = DEFAULT_ROW_SIZE) {
        std::lock_guard<std::mutex> lk(t_seg_mutex_);
        auto seg = std::make_shared<view_segment>(c_start_row, c_start_col);
        c_start_row += rows;
        t_segments[id] = std::move(seg);
    }

    std::shared_ptr<view_segment> get_segment(const std::string& id) {
        std::lock_guard<std::mutex> lk(t_seg_mutex_);
        auto it = t_segments.find(id);
        if (it == t_segments.end()) return nullptr;
        return it->second;
    }

    void start() {
        bool expected = false;
        if (!running_.compare_exchange_strong(expected, true)) {
            return;
        }
        worker_thread = std::thread([this] { this->worker_loop(); });
    }

    void stop() {
        if (!running_.load()) {
            return;
        }
        bool expected = true;
        if (!running_.compare_exchange_strong(expected, false)) {}
        cv_.notify_all();
        if (worker_thread.joinable()) {
            worker_thread.join();
        }
    }

private:
    std::unique_ptr<r_frame> wait_dequeue() {
        std::unique_lock<std::mutex> lock(m_);
        cv_.wait(lock, [this] { return !r_queue.empty() || !running_.load(); });
        if (r_queue.empty()) {
            return nullptr;
        }
        auto item = std::move(r_queue.front());
        r_queue.pop();
        return item;
    }

    static std::string get_readable_speed(const file_size_t bytes_per_sec, const char* suffix = "/s") {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);

        if (bytes_per_sec >= GB_UNIT) {
            oss << static_cast<double>(bytes_per_sec) / GB_UNIT << " GB" << suffix;
        } else if (bytes_per_sec >= MB_UNIT) {
            oss << static_cast<double>(bytes_per_sec) / MB_UNIT << " MB" << suffix;
        } else if (bytes_per_sec >= KB_UNIT) {
            oss << static_cast<double>(bytes_per_sec) / KB_UNIT << " KB" << suffix;
        } else {
            oss << bytes_per_sec << " B" << suffix;
        }

        return oss.str();
    }

    void render(std::unique_ptr<r_frame> frame, const std::shared_ptr<view_segment> seg) {
        move_cursor(seg->t_row, seg->t_col);
        clear_line();
        const auto r_speed_str = get_readable_speed(frame->r_speed);
        {
            std::lock_guard<std::mutex> lk(m_);
            std::cout << "Thread [" << frame->segment_id << "] ";

            if (frame->r_speed == frame->w_speed) {
                std::cout << r_speed_str;
            } else {
                const auto w_speed_str = get_readable_speed(frame->w_speed);
                std::cout << "R: " << r_speed_str << " | W: " << w_speed_str;
            }

            std::cout << std::flush;
        }

        size_t progress_blocks = 0;
        if (frame->total > 0) {
            progress_blocks = static_cast<size_t>(
                (static_cast<double>(frame->copied) / static_cast<double>(frame->total)) * PROGRESS_SIZE + 0.5);
        }
        if (progress_blocks > PROGRESS_SIZE) progress_blocks = PROGRESS_SIZE;

        std::string filled(progress_blocks, FILLED_PROGRESS);
        std::string empty(PROGRESS_SIZE - progress_blocks, EMPTY_PROGRESS);

        {
            std::lock_guard<std::mutex> lk(m_);
            move_cursor(seg->t_row + 1, seg->t_col);
            clear_line();
            std::cout << "[" << filled << empty << "] " << std::flush;
            if (frame->total > 0) {
                double pct = (100.0 * static_cast<double>(frame->copied)) / static_cast<double>(frame->total);
                std::cout << static_cast<int>(pct) << "%" << " (" << get_readable_speed(frame->copied, "") << "/" << get_readable_speed(frame->total, "") << ")" << std::flush;
            } else {
                std::cout << "??%" << std::flush;
            }
        }
    }

    void worker_loop() {
        hide_cursor();
        while (true) {
            auto frame = wait_dequeue();
            if (!frame) break;

            const auto seg = get_segment(frame->segment_id);
            if (!seg) {
                std::lock_guard<std::mutex> lk(m_);
                std::cout << "Unknown segment: " << frame->segment_id << " copied="
                          << frame->copied << "/" << frame->total << std::endl;
                continue;
            }

            render(std::move(frame), seg);
        }
        move_cursor(c_start_row + 1, c_start_col);
        show_cursor();
    }

private:
    int c_start_row{1};
    int c_start_col{1};
    int t_row_size{0};
    int t_col_size{0};

    std::queue<std::unique_ptr<r_frame>> r_queue;
    std::mutex m_;
    std::condition_variable cv_;
    std::atomic<bool> running_{false};
    std::thread worker_thread;

    std::mutex t_seg_mutex_;
    std::unordered_map<std::string, std::shared_ptr<view_segment>> t_segments;
};

#endif //RENDERER_H
