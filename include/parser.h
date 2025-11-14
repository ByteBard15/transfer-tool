#ifndef PARSER_H
#define PARSER_H
#include <ios>
#include <sstream>
#include <string>

#include "constants.h"
#include "types.h"

struct ts_config {
    std::string source_file;
    std::string output_dir;
    std::string filename;
    bool verbose;
    bool show_progress;

    ts_config(): verbose(false), show_progress(false) {}

    std::string to_string() const {
        std::ostringstream ss;
        ss << "Config {\n"
           << "  thread_count: " << "\n"
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
