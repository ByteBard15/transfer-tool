#ifndef PARSER_H
#define PARSER_H
#include <ios>
#include <sstream>
#include <string>

#include "constants.h"
#include "types.h"

struct ts_config {
    int thread_count;
    std::string source_file;
    std::string output_dir;
    file_size_t chunk_size;
    int max_segment_size;
    bool verbose;
    bool show_progress;

    ts_config(): thread_count(DEFAULT_THREAD_COUNT), chunk_size(DEFAULT_CHUNK_SIZE), max_segment_size(MAX_SEGMENT_SIZE), verbose(false), show_progress(false) {}

    std::string to_string() const {
        std::ostringstream ss;
        ss << "Config {\n"
           << "  thread_count: " << thread_count << "\n"
           << "  source_file: \"" << source_file << "\"\n"
           << "  output_dir: \"" << output_dir << "\"\n"
           << "  verbose: " << std::boolalpha << verbose << "\n"
           << "  show_progress: " << std::boolalpha << show_progress << "\n"
           << "}";
        return ss.str();
    }
};

inline std::ostream& operator<<(std::ostream& os, const ts_config& cfg) {
    os << cfg.to_string();
    return os;
}

ts_config parse_args(const int& argc, const char *const argv[]);

#endif
