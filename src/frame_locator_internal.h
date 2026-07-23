#ifndef PRIVATE_MP4_FRAME_LOCATOR_INTERNAL_H_
#define PRIVATE_MP4_FRAME_LOCATOR_INTERNAL_H_

#include <cstddef>
#include <cstdint>

#include "frame_locator.h"
#include "mp4_format.h"

namespace private_mp4 {
namespace detail {

struct ParsedGroup {
  ParsedGroup()
      : group_offset(0u),
        next_group_offset(0u),
        has_next_group(false),
        next_non_audio_group_offset(0u),
        has_next_non_audio(false),
        video_block_count(0u) {}

  GroupHeaderView header;
  std::size_t group_offset;
  std::size_t next_group_offset;
  bool has_next_group;
  GroupHeaderView next_group_header;
  std::size_t next_non_audio_group_offset;
  bool has_next_non_audio;
  GroupHeaderView next_non_audio_header;
  uint32_t video_block_count;
};

/**
 * @brief AlignDown4 rounds a byte offset down to the previous 4-byte boundary.
 * @param value Raw byte offset.
 * @return 4-byte aligned offset.
 * @throws None.
 */
std::size_t AlignDown4(std::size_t value);

/**
 * @brief ClampWindowLower computes a safe lower bound for backward scans.
 * @param center Scan anchor offset.
 * @param window Maximum bytes to scan backwards.
 * @return Lower bound offset.
 * @throws None.
 */
std::size_t ClampWindowLower(std::size_t center, std::size_t window);

/**
 * @brief SafeFrameAdvance computes frame + delta with saturation.
 * @param frame Base frame number.
 * @param delta Non-negative frame delta.
 * @return Saturated frame end value.
 * @throws None.
 */
uint32_t SafeFrameAdvance(uint32_t frame, uint32_t delta);

/**
 * @brief HasGroupStartCodeAt checks whether a byte offset begins with the group start code.
 * @param base Pointer to the mapped file bytes.
 * @param size Total mapped byte count.
 * @param offset Candidate byte offset.
 * @return True when the offset is in range and starts with the encoded group start code.
 * @throws None.
 */
bool HasGroupStartCodeAt(const uint8_t* base,
                         std::size_t size,
                         std::size_t offset);

/**
 * @brief FindPreviousGroupStartCode scans backward for a 4-byte-aligned group start code.
 * @param base Pointer to the mapped file bytes.
 * @param size Total mapped byte count.
 * @param start_offset Inclusive starting offset for the backward scan.
 * @param lower_bound Inclusive lower bound for the scan window.
 * @param out_offset Receives the matching offset on success.
 * @return True when a start code is found in the requested window.
 * @throws None.
 */
bool FindPreviousGroupStartCode(const uint8_t* base,
                                std::size_t size,
                                std::size_t start_offset,
                                std::size_t lower_bound,
                                std::size_t* out_offset);

/**
 * @brief ParseGroupAt validates one group and computes its next byte offset.
 * @param base Pointer to the mapped file bytes.
 * @param size Total mapped byte count.
 * @param group_offset Byte offset where the group should begin.
 * @param out Receives the parsed group metadata on success.
 * @return True when the whole group is structurally valid.
 * @throws None.
 */
bool ParseGroupAt(const uint8_t* base,
                  std::size_t size,
                  std::size_t group_offset,
                  ParsedGroup* out);

/**
 * @brief FindNextNonAudioGroup parses forward until a non-audio group is found.
 * @param base Pointer to the mapped file bytes.
 * @param size Total mapped byte count.
 * @param group Parsed current group to extend.
 * @return True when a next non-audio group exists.
 * @throws None.
 */
bool FindNextNonAudioGroup(const uint8_t* base,
                           std::size_t size,
                           ParsedGroup* group);

/**
 * @brief ComputeGroupFrameSpan derives how many frames a non-audio group covers.
 * @param group Parsed group metadata.
 * @return Number of frames covered by the current group.
 * @throws None.
 */
uint32_t ComputeGroupFrameSpan(const ParsedGroup& group);

/**
 * @brief HasImmediateSharedFrame checks whether the next immediate group shares the same frame.
 * @param group Parsed current group metadata.
 * @return True when the next group is non-audio and carries the same frame_num.
 * @throws None.
 */
bool HasImmediateSharedFrame(const ParsedGroup& group);

/**
 * @brief GroupContainsFrame checks whether a parsed group covers the target frame.
 * @param group Parsed current group metadata.
 * @param target_frame Requested frame number.
 * @return True when the frame belongs to this group.
 * @throws None.
 */
bool GroupContainsFrame(const ParsedGroup& group, uint32_t target_frame);

/**
 * @brief AppendMatch appends a located block result to the fixed-size output.
 * @param parsed_group Parsed group that contains the frame.
 * @param target_frame Requested frame number.
 * @param block_index_in_group Zero-based block index inside the group.
 * @param block_header_offset Byte offset of the block header.
 * @param continuation Whether the block is a continuation of a previous match.
 * @param out Receives the appended result.
 * @return True when the result is appended successfully.
 * @throws None.
 */
bool AppendMatch(const ParsedGroup& parsed_group,
                 uint32_t target_frame,
                 uint32_t block_index_in_group,
                 std::size_t block_header_offset,
                 bool continuation,
                 SearchMatches* out);

/**
 * @brief AppendFrameMatchFromGroup maps a frame to a block inside a parsed group.
 * @param base Pointer to the mapped file bytes.
 * @param size Total mapped byte count.
 * @param parsed_group Parsed group that contains the frame.
 * @param target_frame Requested frame number.
 * @param continuation Whether the match continues a previous one.
 * @param out Receives the appended result.
 * @return True when a matching block is found.
 * @throws None.
 */
bool AppendFrameMatchFromGroup(const uint8_t* base,
                               std::size_t size,
                               const ParsedGroup& parsed_group,
                               uint32_t target_frame,
                               bool continuation,
                               SearchMatches* out);

/**
 * @brief FindPreviousGroupEndingAt searches backwards for the group that ends at a known offset.
 * @param base Pointer to the mapped file bytes.
 * @param size Total mapped byte count.
 * @param end_offset Byte offset where the candidate previous group must end.
 * @param max_scan_bytes Maximum backward scan window.
 * @param out Receives the previous group on success.
 * @return True when a previous valid group is found.
 * @throws None.
 */
bool FindPreviousGroupEndingAt(const uint8_t* base,
                               std::size_t size,
                               std::size_t end_offset,
                               std::size_t max_scan_bytes,
                               ParsedGroup* out);

/**
 * @brief FindContainingGroup scans backward for the valid group that covers a byte offset.
 * @param base Pointer to the mapped file bytes.
 * @param size Total mapped byte count.
 * @param offset Arbitrary byte offset inside the file.
 * @param window_bytes Maximum backward scan distance.
 * @param out Receives the containing group on success.
 * @return True when a containing valid group is found.
 * @throws None.
 */
bool FindContainingGroup(const uint8_t* base,
                         std::size_t size,
                         std::size_t offset,
                         std::size_t window_bytes,
                         ParsedGroup* out);

}  // namespace detail
}  // namespace private_mp4

#endif  // PRIVATE_MP4_FRAME_LOCATOR_INTERNAL_H_
