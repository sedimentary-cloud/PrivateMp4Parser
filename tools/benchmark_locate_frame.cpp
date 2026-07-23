#include "frame_locator.h"
#include "mapped_file_win32.h"
#include "synthetic_mp4_support.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace private_mp4 {

struct BenchmarkStats {
  double average_us;
  double p95_us;
  double p99_us;
  double max_us;
  std::size_t sample_count;
};

/**
 * @brief ParseUint32 parses a decimal unsigned integer from a command-line token.
 * @param text Decimal text to parse.
 * @param value Receives the parsed value on success.
 * @return True when parsing succeeds.
 * @throws None.
 */
bool ParseUint32(const std::string& text, uint32_t* value) {
  if (value == NULL || text.empty()) {
    return false;
  }

  uint64_t parsed = 0u;
  for (std::size_t i = 0u; i < text.size(); ++i) {
    if (text[i] < '0' || text[i] > '9') {
      return false;
    }
    parsed = parsed * 10u + static_cast<uint64_t>(text[i] - '0');
    if (parsed > 0xFFFFFFFFull) {
      return false;
    }
  }

  *value = static_cast<uint32_t>(parsed);
  return true;
}

/**
 * @brief PercentileValue returns a percentile from a sorted sample vector.
 * @param sorted_us Sorted latency samples in microseconds.
 * @param percentile Percentile in range [0, 100].
 * @return Selected latency sample.
 * @throws None.
 */
double PercentileValue(const std::vector<double>& sorted_us, double percentile) {
  if (sorted_us.empty()) {
    return 0.0;
  }

  const double position = (percentile / 100.0) * static_cast<double>(sorted_us.size() - 1u);
  std::size_t index = static_cast<std::size_t>(position);
  if (index >= sorted_us.size()) {
    index = sorted_us.size() - 1u;
  }
  return sorted_us[index];
}

/**
 * @brief MeasureLocateFrame runs repeated LocateFrame calls and aggregates timings.
 * @param base Pointer to mapped file bytes.
 * @param size Total mapped byte count.
 * @param frame_count Upper bound of queried frames.
 * @param iterations Number of benchmark iterations.
 * @param options Search tuning knobs for LocateFrame.
 * @param stats Receives aggregate latency statistics.
 * @return True when every query is found successfully.
 * @throws None.
 */
bool MeasureLocateFrame(const uint8_t* base,
                        std::size_t size,
                        uint32_t frame_count,
                        uint32_t iterations,
                        const SearchOptions& options,
                        BenchmarkStats* stats) {
  if (stats == NULL || frame_count == 0u || iterations == 0u) {
    return false;
  }

  std::vector<double> samples_us;
  const uint32_t warmup_iterations =
      (frame_count < 4096u) ? frame_count : 4096u;
  const uint32_t batch_size = 64u;
  samples_us.reserve((iterations / batch_size) + 1u);

  for (uint32_t i = 0u; i < warmup_iterations; ++i) {
    const uint32_t target_frame =
        (i * 2654435761u) % frame_count;
    SearchMatches matches;
    if (!LocateFrame(base, size, target_frame, &matches, options) ||
        matches.count == 0u) {
      return false;
    }
  }

  for (uint32_t begin = 0u; begin < iterations; begin += batch_size) {
    const uint32_t end =
        ((iterations - begin) < batch_size) ? iterations : (begin + batch_size);
    const std::chrono::high_resolution_clock::time_point start_time =
        std::chrono::high_resolution_clock::now();
    for (uint32_t i = begin; i < end; ++i) {
      const uint32_t target_frame =
          (i * 2654435761u) % frame_count;
      SearchMatches matches;
      const bool found = LocateFrame(base, size, target_frame, &matches, options);
      if (!found || matches.count == 0u) {
        return false;
      }
    }
    const std::chrono::high_resolution_clock::time_point end_time =
        std::chrono::high_resolution_clock::now();
    const std::chrono::duration<double, std::nano> elapsed = end_time - start_time;
    const double average_call_us =
        (elapsed.count() / static_cast<double>(end - begin)) / 1000.0;
    samples_us.push_back(average_call_us);
  }

  std::sort(samples_us.begin(), samples_us.end());
  double sum_us = 0.0;
  for (std::size_t i = 0u; i < samples_us.size(); ++i) {
    sum_us += samples_us[i];
  }

  stats->average_us = sum_us / static_cast<double>(samples_us.size());
  stats->p95_us = PercentileValue(samples_us, 95.0);
  stats->p99_us = PercentileValue(samples_us, 99.0);
  stats->max_us = samples_us.back();
  stats->sample_count = samples_us.size();
  return true;
}

/**
 * @brief PrintUsage prints the benchmark command-line syntax.
 * @param argv0 Executable path used in the invocation.
 * @return None.
 * @throws None.
 */
void PrintUsage(const char* argv0) {
  std::cerr << "Usage: " << argv0
            << " <sample_path> [target_frames] [iterations] [probe_window_bytes] [min_payload_bytes] [max_payload_bytes]\n";
}

}  // namespace private_mp4

int main(int argc, char** argv) {
  if (argc < 2 || argc > 7) {
    private_mp4::PrintUsage(argv[0]);
    return EXIT_FAILURE;
  }

  private_mp4::SyntheticFileConfig config;
  uint32_t iterations = 20000u;
  private_mp4::SearchOptions options;
  uint32_t probe_window_bytes = static_cast<uint32_t>(options.probe_window_bytes);

  if (argc >= 3 &&
      !private_mp4::ParseUint32(argv[2], &config.target_video_frames)) {
    std::cerr << "Invalid target_frames value.\n";
    return EXIT_FAILURE;
  }
  if (argc >= 4 &&
      !private_mp4::ParseUint32(argv[3], &iterations)) {
    std::cerr << "Invalid iterations value.\n";
    return EXIT_FAILURE;
  }
  if (argc >= 5 &&
      !private_mp4::ParseUint32(argv[4], &probe_window_bytes)) {
    std::cerr << "Invalid probe_window_bytes value.\n";
    return EXIT_FAILURE;
  }
  if (argc >= 6 &&
      !private_mp4::ParseUint32(argv[5], &config.min_payload_size_bytes)) {
    std::cerr << "Invalid min_payload_bytes value.\n";
    return EXIT_FAILURE;
  }
  if (argc >= 7 &&
      !private_mp4::ParseUint32(argv[6], &config.max_payload_size_bytes)) {
    std::cerr << "Invalid max_payload_bytes value.\n";
    return EXIT_FAILURE;
  }
  if (argc < 7) {
    config.max_payload_size_bytes = config.min_payload_size_bytes;
  }
  options.probe_window_bytes = probe_window_bytes;

  std::size_t generated_file_size = 0u;
  if (!private_mp4::WriteSyntheticFile(argv[1], config, &generated_file_size)) {
    std::cerr << "Failed to generate benchmark sample file.\n";
    return EXIT_FAILURE;
  }

  private_mp4::MappedFileWin32 mapped_file;
  if (!mapped_file.Open(argv[1])) {
    std::cerr << "Failed to map sample file: " << mapped_file.error_message() << "\n";
    return EXIT_FAILURE;
  }

  private_mp4::BenchmarkStats stats = {};
  if (!private_mp4::MeasureLocateFrame(mapped_file.data(),
                                       mapped_file.size(),
                                       config.target_video_frames,
                                       iterations,
                                       options,
                                       &stats)) {
    std::cerr << "Benchmark failed because LocateFrame did not find a query.\n";
    return EXIT_FAILURE;
  }

  std::cout << std::fixed << std::setprecision(3)
            << "sample_path=" << argv[1]
            << " frames=" << config.target_video_frames
            << " iterations=" << iterations
            << " samples=" << stats.sample_count
            << " file_size_bytes=" << generated_file_size
            << " min_payload_bytes=" << config.min_payload_size_bytes
            << " max_payload_bytes=" << config.max_payload_size_bytes
            << " avg_us=" << stats.average_us
            << " p95_us=" << stats.p95_us
            << " p99_us=" << stats.p99_us
            << " max_us=" << stats.max_us
            << "\n";

  return EXIT_SUCCESS;
}
