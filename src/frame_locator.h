#ifndef PRIVATE_MP4_FRAME_LOCATOR_H_
#define PRIVATE_MP4_FRAME_LOCATOR_H_

#include <cstddef>
#include <cstdint>

namespace private_mp4 {

struct SearchResult {
  uint32_t target_frame;
  uint32_t group_start_frame;
  uint32_t next_group_start_frame;
  uint32_t group_frame_span;
  uint32_t block_index_in_group;
  std::size_t group_file_offset;
  std::size_t block_header_offset;
  std::size_t block_data_offset;
  bool found;
  bool continuation;
};

struct SearchMatches {
  SearchMatches() : count(0u) {}

  SearchResult items[2];
  std::size_t count;
};

struct SearchOptions {
  SearchOptions()
      : probe_window_bytes(512u * 1024u),
        backward_scan_bytes(1024u * 1024u) {}

  std::size_t probe_window_bytes;
  std::size_t backward_scan_bytes;
};

/**
 * @brief LocateFrame finds the group and block offsets for a target frame.
 * @param base Pointer to the mapped private MP4 bytes.
 * @param size Total byte size of the mapped file.
 * @param target_frame Requested frame number.
 * @param out Receives one or two matches on success.
 * @param options Optional tuning knobs for probe and fallback ranges.
 * @return True when at least one matching block is found.
 * @throws None.
 */
bool LocateFrame(const uint8_t* base,
                 std::size_t size,
                 uint32_t target_frame,
                 SearchMatches* out,
                 const SearchOptions& options = SearchOptions());

}  // namespace private_mp4

#endif  // PRIVATE_MP4_FRAME_LOCATOR_H_
