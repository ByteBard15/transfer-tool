//
// Created by bytebard on 10/23/25.
//

#include <filesystem>

#include "file.h"
#include "logger.h"
#include "parser.h"
#include "types.h"

int main(const int argc, const char *const argv[]) {
    logger s_logger;
    s_logger.info("Logging arguments", argc, argv);
    for (auto i = 1; i < argc; i++) {
        s_logger.info("Argument", i, argv[i]);
    }
    const ts_config config = parse_args(argc, argv);
    s_logger.info("Parsed config", config);

    std::filesystem::path src_path(config.source_file);

    if (!std::filesystem::exists(src_path)) {
        throw std::runtime_error("No source file");
    }

    std::filesystem::path destination_path(config.output_dir);
    if (!std::filesystem::exists(destination_path)) {
        throw std::runtime_error("No destination file");
    }

    const std::filesystem::file_status src_status = std::filesystem::status(src_path);
    const auto src_type = src_status.type();
    if (src_type != std::filesystem::file_type::regular) {
        throw std::runtime_error("File type not handled");
    }

    const std::filesystem::file_status dest_status = std::filesystem::status(destination_path);
    const auto dest_type = dest_status.type();
    if (dest_type != std::filesystem::file_type::directory) {
        throw std::runtime_error("Destination path is not a directory");
    }

    s_logger.info("Source file", src_path);
    s_logger.info("Destination file", destination_path);

    file_size_t total_file_size = std::filesystem::file_size(src_path);
    if (total_file_size < config.chunk_size) {
        throw std::runtime_error("Chunk size is too small");
    }

    // auto segment_size = static_cast<unsigned int>(total_file_size / config.chunk_size);
    const int checksum_size = get_checksum_chunk_size(total_file_size);
    s_logger.info("Checksum size", checksum_size);
    const std::string checksum = compute_checksum(src_path, checksum_size);

    transfer_status status;
    status.destination_file = std::move(destination_path);
    status.source_file = std::move(src_path);
    status.total_size = total_file_size;
    status.checksum = checksum;

    s_logger.info("Initiating transfer", status);

    // Check checksum

    return 0;
}
