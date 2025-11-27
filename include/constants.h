#ifndef CONSTANTS_H
#define CONSTANTS_H
#include "types.h"

#define DEFAULT_THREAD_COUNT 2
#define TRANSFER_STATUS_FILE "{}_Atomic_status.json"

constexpr file_size_t KB_UNIT = 1024ULL;
constexpr file_size_t MB_UNIT = KB_UNIT * 1024ULL;
constexpr file_size_t GB_UNIT = MB_UNIT * 1024ULL;

constexpr file_size_t convert_to_bytes(file_size_t size, file_size_t unit) {
    return size * unit;
}

// Checksum
constexpr file_size_t CHECKSUM_UPPER_BOUND = convert_to_bytes(64, KB_UNIT);
constexpr file_size_t CHECKSUM_LOWER_BOUND = convert_to_bytes(8, MB_UNIT);
constexpr file_size_t CHECKSUM_SCALE_FACTOR = 64;
constexpr file_size_t MAX_CHECKSUM_FILE_SIZE = convert_to_bytes(20, MB_UNIT);

// File segment size
constexpr file_size_t MAX_FILE_SEGMENTS = convert_to_bytes(4, GB_UNIT);
constexpr file_size_t MIN_FILE_SEGMENTS = convert_to_bytes(4, KB_UNIT);
constexpr file_size_t SEGMENT_SCALE_FACTOR = 64;

constexpr file_size_t MIN_BUF_SIZE = convert_to_bytes(64, KB_UNIT);
constexpr file_size_t MAX_BUF_SIZE = convert_to_bytes(60, GB_UNIT);
constexpr file_size_t BASELINE_BUF_SIZE = convert_to_bytes(4, MB_UNIT);
constexpr double HIGH_LATENCY_NS = 20e6;

constexpr file_size_t UPDATE_THRESHOLD = convert_to_bytes(5, MB_UNIT);
constexpr file_size_t CONFIG_UPDATE_THRESHOLD = convert_to_bytes(5, MB_UNIT);

#endif