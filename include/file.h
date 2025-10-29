#ifndef FILE_H
#define FILE_H
#include <fstream>
#include <string>

#include "types.h"
#include "json.hpp"
#include <openssl/evp.h>

#include "constants.h"

struct segment_status {
    std::string segment_name;
    file_size_t start_byte;
    file_size_t end_byte;
    file_size_t last_copied_byte;
    bool completed;

    segment_status()
        : start_byte(0), end_byte(0), last_copied_byte(0), completed(false) {}

    nlohmann::json to_json() const {
        return {
                    {"segment_name", segment_name},
                    {"start_byte", start_byte},
                    {"end_byte", end_byte},
                    {"last_copied_byte", last_copied_byte},
                    {"completed", completed}
        };
    }

    static segment_status from_json(const nlohmann::json& j) {
        segment_status seg;
        seg.segment_name = j.value("segment_name", "");
        seg.start_byte = j.value("start_byte", 0ULL);
        seg.end_byte = j.value("end_byte", 0ULL);
        seg.last_copied_byte = j.value("last_copied_byte", 0ULL);
        seg.completed = j.value("completed", false);
        return seg;
    }
};

struct transfer_status {
    std::string source_file;
    std::string destination_file;
    file_size_t total_size;
    file_size_t total_copied;
    std::string checksum;
    std::vector<segment_status> segments;

    transfer_status()
        : total_size(0), total_copied(0) {}

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["source_file"] = source_file;
        j["destination_file"] = destination_file;
        j["total_size"] = total_size;
        j["total_copied"] = total_copied;
        j["checksum"] = checksum;

        j["segments"] = nlohmann::json::array();
        for (const auto& seg : segments) {
            j["segments"].push_back(seg.to_json());
        }

        return j;
    }

    static transfer_status from_json(const nlohmann::json& j) {
        transfer_status ts;
        ts.source_file = j.value("source_file", "");
        ts.destination_file = j.value("destination_file", "");
        ts.total_size = j.value("total_size", 0ULL);
        ts.total_copied = j.value("total_copied", 0ULL);
        ts.checksum = j.value("checksum", "");

        if (j.contains("segments") && j["segments"].is_array()) {
            for (const auto& item : j["segments"]) {
                ts.segments.push_back(segment_status::from_json(item));
            }
        }

        return ts;
    }
};

inline std::ostream& operator<<(std::ostream& os, const segment_status& ss) {
    return os << ss.to_json();
}

inline std::ostream& operator<<(std::ostream& os, const transfer_status& ts) {
    return os << ts.to_json();
}

inline int get_checksum_chunk_size(const file_size_t& file_size) {
    if (file_size <= CHECKSUM_LOWER_BOUND) {
        return static_cast<int>(file_size);
    }

    if (file_size >= MAX_CHECKSUM_FILE_SIZE) {
        return CHECKSUM_UPPER_BOUND;
    }

    const int proportional = static_cast<int>(file_size / CHECKSUM_SCALE_FACTOR);
    return std::min(proportional, CHECKSUM_UPPER_BOUND);
}

inline std::string compute_checksum(const std::string& path, const int& chunk_size) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Could not open file " + path);
    }

    std::streamsize file_size = file.tellg();

    if (file_size == chunk_size) {
        std::vector<unsigned char> buffer(file_size);
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(buffer.data()), file_size);

        unsigned char result[EVP_MAX_MD_SIZE];
        unsigned int result_len = 0;

        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        EVP_DigestInit_ex(ctx, EVP_md5(), nullptr);
        EVP_DigestUpdate(ctx, buffer.data(), buffer.size());
        EVP_DigestFinal_ex(ctx, result, &result_len);
        EVP_MD_CTX_free(ctx);

        std::ostringstream res;
        for (unsigned char i : result)
            res << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(i);
        return res.str();
    }

    std::vector<unsigned char> buffer(2 * chunk_size);

    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char *>(&buffer[0]), chunk_size);

    file.seekg(file_size - chunk_size, std::ios::beg);
    file.read(reinterpret_cast<char *>(&buffer[chunk_size]), chunk_size);

    unsigned char result[EVP_MAX_MD_SIZE];
    unsigned int result_len = 0;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
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

#endif
