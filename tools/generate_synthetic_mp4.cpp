#include "synthetic_mp4_support.h"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace private_mp4 {

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
 * @brief PrintUsage prints the generator command-line syntax.
 * @param argv0 Executable path used in the invocation.
 * @return None.
 * @throws None.
 */
void PrintUsage(const char* argv0) {
  std::cerr << "Usage: " << argv0
            << " <output_path> [target_frames] [audio_period] [min_payload_bytes] [max_payload_bytes]\n";
}

}  // namespace private_mp4

int main(int argc, char** argv) {
  if (argc < 2 || argc > 6) {
    private_mp4::PrintUsage(argv[0]);
    return EXIT_FAILURE;
  }

  private_mp4::SyntheticFileConfig config;
  if (argc >= 3 &&
      !private_mp4::ParseUint32(argv[2], &config.target_video_frames)) {
    std::cerr << "Invalid target_frames value.\n";
    return EXIT_FAILURE;
  }
  if (argc >= 4 &&
      !private_mp4::ParseUint32(argv[3], &config.audio_group_period)) {
    std::cerr << "Invalid audio_period value.\n";
    return EXIT_FAILURE;
  }
  if (argc >= 5 &&
      !private_mp4::ParseUint32(argv[4], &config.min_payload_size_bytes)) {
    std::cerr << "Invalid min_payload_bytes value.\n";
    return EXIT_FAILURE;
  }
  if (argc >= 6 &&
      !private_mp4::ParseUint32(argv[5], &config.max_payload_size_bytes)) {
    std::cerr << "Invalid max_payload_bytes value.\n";
    return EXIT_FAILURE;
  }
  if (argc < 6) {
    config.max_payload_size_bytes = config.min_payload_size_bytes;
  }

  std::size_t file_size_bytes = 0u;
  if (!private_mp4::WriteSyntheticFile(argv[1], config, &file_size_bytes)) {
    std::cerr << "Failed to write synthetic private MP4 file.\n";
    return EXIT_FAILURE;
  }

  std::cout << "output=" << argv[1]
            << " target_frames=" << config.target_video_frames
            << " audio_period=" << config.audio_group_period
            << " min_payload_bytes=" << config.min_payload_size_bytes
            << " max_payload_bytes=" << config.max_payload_size_bytes
            << " file_size_bytes=" << file_size_bytes << "\n";
  return EXIT_SUCCESS;
}
