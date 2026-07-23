#include "frame_locator.h"
#include "frame_query.h"
#include "mapped_file_win32.h"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace private_mp4 {

/**
 * @brief FormatHexOffset formats a byte offset as uppercase hexadecimal.
 * @param offset Byte offset to print.
 * @return String such as 0x6C.
 * @throws None.
 */
std::string FormatHexOffset(std::size_t offset) {
  std::ostringstream stream;
  stream << "0x" << std::uppercase << std::hex << offset;
  return stream.str();
}

/**
 * @brief PrintUsage prints the expected command-line syntax.
 * @param argv0 Executable path used in the invocation.
 * @return None.
 * @throws None.
 */
void PrintUsage(const char* argv0) {
  std::cerr << "Usage: " << argv0 << " <private_mp4_path> <frame_query>\n";
  std::cerr << "Example: " << argv0 << " sample.mp4 1,5:8,42\n";
}

/**
 * @brief PrintMatches prints the located frame results in a stable text format.
 * @param target_frame Requested frame number.
 * @param matches Located one or two match entries.
 * @param elapsed_us Measured core search time in microseconds.
 * @return None.
 * @throws None.
 */
void PrintMatches(uint32_t target_frame,
                  const SearchMatches& matches,
                  double elapsed_us) {
  if (matches.count == 0u) {
    std::cout << "frame=" << target_frame
              << " status=not_found"
              << " lookup_us=" << std::fixed << std::setprecision(3)
              << elapsed_us << "\n";
    return;
  }

  std::cout << "frame=" << target_frame
            << " status=found"
            << " match_count=" << matches.count
            << " lookup_us=" << std::fixed << std::setprecision(3)
            << elapsed_us << "\n";

  for (std::size_t index = 0u; index < matches.count; ++index) {
    const SearchResult& result = matches.items[index];
    std::cout << "  match[" << index << "]"
              << " group_offset=" << FormatHexOffset(result.group_file_offset)
              << " block_header_offset=" << FormatHexOffset(result.block_header_offset)
              << " block_data_offset=" << FormatHexOffset(result.block_data_offset)
              << " group_start_frame=" << result.group_start_frame
              << " group_frame_span=" << result.group_frame_span
              << " block_index=" << result.block_index_in_group
              << " continuation=" << (result.continuation ? "true" : "false")
              << "\n";
  }
}

}  // namespace private_mp4

int main(int argc, char** argv) {
  if (argc != 3) {
    private_mp4::PrintUsage(argv[0]);
    return EXIT_FAILURE;
  }

  const std::string file_path = argv[1];
  const std::string frame_query = argv[2];

  std::vector<uint32_t> frames;
  std::string parse_error;
  if (!private_mp4::ParseFrameQuery(frame_query, &frames, &parse_error)) {
    std::cerr << "Frame query parse failed: " << parse_error << "\n";
    return EXIT_FAILURE;
  }

  private_mp4::MappedFileWin32 mapped_file;
  if (!mapped_file.Open(file_path)) {
    std::cerr << "Failed to open file: " << mapped_file.error_message() << "\n";
    return EXIT_FAILURE;
  }

  int exit_code = EXIT_SUCCESS;
  for (std::size_t index = 0u; index < frames.size(); ++index) {
    const uint32_t target_frame = frames[index];

    private_mp4::SearchMatches matches;
    const std::chrono::high_resolution_clock::time_point start_time =
        std::chrono::high_resolution_clock::now();
    const bool found = private_mp4::LocateFrame(mapped_file.data(),
                                                mapped_file.size(),
                                                target_frame,
                                                &matches);
    const std::chrono::high_resolution_clock::time_point end_time =
        std::chrono::high_resolution_clock::now();

    const std::chrono::duration<double, std::micro> elapsed = end_time - start_time;
    private_mp4::PrintMatches(target_frame, matches, elapsed.count());
    if (!found) {
      exit_code = EXIT_FAILURE;
    }
  }

  return exit_code;
}
