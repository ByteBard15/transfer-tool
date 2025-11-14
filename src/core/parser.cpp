#include "parser.h"

#include <complex>
#include <filesystem>
#include <stdexcept>
#include <unordered_set>

void skip_whitespace(const char *(&arg), int &ptr) {
    while (arg[ptr] == ' ' || arg[ptr] == '\t') {
        ptr++;
    }
}

std::string read_name(const char *(&arg), int &ptr) {
    std::string name;
    while (arg[ptr] != '\0' && arg[ptr] != ' ' && arg[ptr] != '\t' && arg[ptr] != '\n' && arg[ptr] != '=') {
        name.append(1, arg[ptr++]);
    }
    return name;
}

void set_config(ts_config &config, const std::string &name, std::string &value) {
    if (name == "-p" || name == "--progress") {
        config.show_progress = true;
        return;
    }
    if (name == "--source" || name == "-s") {
        config.source_file = std::move(value);
        return;
    }
    if (name == "--destination" || name == "-d") {
        config.output_dir = std::move(value);
        return;
    }
    throw std::invalid_argument("Unknown config option '" + name + "'");
}

ts_config parse_args(const int& argc, const char *const argv[]) {
    ts_config config;
    std::pmr::unordered_set<std::string> visited;
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        int arg_i = 0;
        skip_whitespace(arg, arg_i);
        std::string name = read_name(arg, arg_i);
        if (visited.contains(name)) {
            throw std::invalid_argument("Config option '" + name + "' already exists");
        }
        visited.insert(name);
        std::string value;
        if (arg[arg_i] == '=') {
            value = read_name(arg, ++arg_i);
        }

        if (name == "-p" || name == "--progress") {
            config.show_progress = value == "true" || value.empty();
            continue;
        }

        if (i < argc && value.empty()) {
            value = argv[++i];
        }
        if (name == "-f" || name == "--filename") {
            config.filename = value;
            continue;
        }
        if (name == "--source" || name == "-s") {
            config.source_file = std::move(value);
            continue;
        }
        if (name == "--destination" || name == "-d") {
            config.output_dir = std::move(value);
            continue;
        }
        throw std::invalid_argument("Unknown config option '" + name + "'");
    }

    return config;
}
