#include "frame_locator.h"

#include "frame_locator_internal.h"
#include "mp4_format.h"

namespace private_mp4 {

namespace {

/**
 * @brief CollectMatches expands a located group into one or two output results.
 * @param base Pointer to the mapped file bytes.
 * @param size Total mapped byte count.
 * @param located_group Parsed group selected by binary or linear search.
 * @param target_frame Requested frame number.
 * @param options Search tuning knobs.
 * @param out Receives the matches.
 * @return True when at least one match is produced.
 * @throws None.
 */
bool CollectMatches(const uint8_t* base,
                    std::size_t size,
                    const detail::ParsedGroup& located_group,
                    uint32_t target_frame,
                    const SearchOptions& options,
                    SearchMatches* out) {
  if (out == NULL) {
    return false;
  }

  out->count = 0u;

  detail::ParsedGroup previous_group;
  if (detail::FindPreviousGroupEndingAt(base,
                                        size,
                                        located_group.group_offset,
                                        options.backward_scan_bytes,
                                        &previous_group) &&
      !previous_group.header.is_audio &&
      previous_group.header.frame_num == target_frame &&
      located_group.header.frame_num == target_frame) {
    detail::FindNextNonAudioGroup(base, size, &previous_group);
    detail::AppendFrameMatchFromGroup(base, size, previous_group, target_frame, false, out);
    detail::AppendFrameMatchFromGroup(base, size, located_group, target_frame, true, out);
    return out->count > 0u;
  }

  detail::AppendFrameMatchFromGroup(base, size, located_group, target_frame, false, out);
  if (detail::HasImmediateSharedFrame(located_group)) {
    detail::ParsedGroup next_group;
    if (detail::ParseGroupAt(base, size, located_group.next_group_offset, &next_group)) {
      detail::FindNextNonAudioGroup(base, size, &next_group);
      detail::AppendFrameMatchFromGroup(base, size, next_group, target_frame, true, out);
    }
  }

  return out->count > 0u;
}

/**
 * @brief LocateFrameLinear provides a correctness-first fallback full scan.
 * @param base Pointer to the mapped file bytes.
 * @param size Total mapped byte count.
 * @param target_frame Requested frame number.
 * @param options Search tuning knobs.
 * @param out Receives located matches.
 * @return True when the frame is found.
 * @throws None.
 */
bool LocateFrameLinear(const uint8_t* base,
                       std::size_t size,
                       uint32_t target_frame,
                       const SearchOptions& options,
                       SearchMatches* out) {
  std::size_t offset = kFileHeaderSize;
  while (offset + kGroupHeaderSize <= size) {
    detail::ParsedGroup group;
    if (!detail::ParseGroupAt(base, size, offset, &group)) {
      return false;
    }
    detail::FindNextNonAudioGroup(base, size, &group);
    if (detail::GroupContainsFrame(group, target_frame)) {
      return CollectMatches(base, size, group, target_frame, options, out);
    }
    if (!group.header.is_audio && target_frame < group.header.frame_num) {
      break;
    }
    offset = group.next_group_offset;
  }

  out->count = 0u;
  return false;
}

}  // namespace

bool LocateFrame(const uint8_t* base,
                 std::size_t size,
                 uint32_t target_frame,
                 SearchMatches* out,
                 const SearchOptions& options) {
  if (out == NULL) {
    return false;
  }
  out->count = 0u;

  FileHeaderView file_header;
  if (!ParseFileHeader(base, size, &file_header)) {
    return false;
  }

  const std::size_t first_group_offset = kFileHeaderSize;
  if (size < first_group_offset + kGroupHeaderSize) {
    return false;
  }

  std::size_t left = first_group_offset;
  std::size_t right = size - 1u;
  for (int iteration = 0; iteration < 48 && left < right; ++iteration) {
    const std::size_t mid = left + ((right - left) / 2u);
    detail::ParsedGroup group;
    if (!detail::FindContainingGroup(base,
                                     size,
                                     mid,
                                     options.probe_window_bytes,
                                     &group)) {
      break;
    }

    detail::FindNextNonAudioGroup(base, size, &group);

    if (detail::GroupContainsFrame(group, target_frame)) {
      return CollectMatches(base, size, group, target_frame, options, out);
    }

    if (group.header.is_audio || target_frame >= group.header.frame_num) {
      if (group.next_group_offset <= left) {
        break;
      }
      left = group.next_group_offset;
      continue;
    }

    if (group.group_offset == 0u || group.group_offset - 1u >= right) {
      break;
    }
    right = group.group_offset - 1u;
  }

  return LocateFrameLinear(base, size, target_frame, options, out);
}

}  // namespace private_mp4
