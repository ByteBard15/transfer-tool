#include "file.h"
#include "logger.h"
#include "parser.h"

std::shared_ptr<file_transfer_status> get_status(
    const std::filesystem::path& destination_path,
    const std::string& filename,
    const std::string& src_path,
    const unsigned long thread_count,
    const file_size_t& total_file_size,
    const std::string& checksum
) {
    std::string destination_file = destination_path / filename;
    std::shared_ptr<file_transfer_status> status = std::make_shared<file_transfer_status>(src_path,
                                destination_file, total_file_size, 0, checksum);

    file_size_t start_byte = 0;
    const auto s_size_props = get_segment_size(total_file_size, thread_count);
    const file_size_t segment_size = s_size_props.first;
    const file_size_t remainder = s_size_props.second;
    status->segments.reserve(thread_count);

    for (int i = 0; i < thread_count; i++) {
        file_size_t size = segment_size + (remainder > i ? 1 : 0);
        auto end_byte = start_byte + std::min(size, total_file_size);
        std::shared_ptr<segment_status> seg = std::make_shared<segment_status>(start_byte, end_byte);
        status->segments[generate_uuid()] = std::move(seg);
        start_byte = end_byte;
    }
    prepare_target_file(status->destination_file, status->total_size);

    return status;
}

int main(const int argc, const char *const argv[]) {
    logger s_logger;
    s_logger.info("Logging arguments", argc, argv);
    for (auto i = 1; i < argc; i++) {
        s_logger.info("Argument", i, argv[i]);
    }
    ts_config config = parse_args(argc, argv);

    const auto thread_count = compute_optimal_threads();

    std::string filename = config.filename;
    if (filename.empty()) {
        filename = std::filesystem::path(config.source_file).filename();
    }

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

    const file_size_t total_file_size = std::filesystem::file_size(src_path);

    const file_size_t checksum_size = get_checksum_chunk_size(total_file_size);
    const std::string checksum = compute_checksum(src_path, checksum_size);

    const auto config_file_path = destination_path / get_config_filename(filename, checksum);
    const std::filesystem::file_status config_status = std::filesystem::status(config_file_path);
    const auto config_type = config_status.type();

    bool create_json_file = true;
    std::shared_ptr<file_transfer_status> status = nullptr;
    if (config_type != std::filesystem::file_type::not_found) {
        const auto json = get_json(config_file_path);
        status = file_transfer_status::from_json(json);
        if (status->checksum == checksum || status->total_size == total_file_size) {
            create_json_file = false;
        }
    }
    if (create_json_file) {
        status = get_status(destination_path, filename, src_path, thread_count, total_file_size, checksum);
        std::ofstream json_file(config_file_path);
        json_file << status->to_json();
    }
    write_segments(status);

    return 0;
}
