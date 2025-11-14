#include "file.h"
#include "logger.h"
#include "parser.h"

int main(const int argc, const char *const argv[]) {
    logger s_logger;
    s_logger.info("Logging arguments", argc, argv);
    for (auto i = 1; i < argc; i++) {
        s_logger.info("Argument", i, argv[i]);
    }
    ts_config config = parse_args(argc, argv);

    const auto thread_count = compute_optimal_threads();

    auto status = get_transfer_status(config, s_logger, thread_count);
    prepare_target_file(status.destination_file, status.total_size);

    s_logger.info("Initiating transfer", status);
    write_segments(status);

    return 0;
}
