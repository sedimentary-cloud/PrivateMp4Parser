#include "frame_locator_internal.h"

#include <algorithm>
#include <limits>

namespace private_mp4 {
namespace detail {

namespace {

static const std::size_t kFirstGroupOffset = kFileHeaderSize;
static const uint32_t kMaxReasonableBlockCount = 4096u;

}  // namespace

std::size_t AlignDown4(std::size_t value) {
  return value & static_cast<std::size_t>(~std::size_t(0x3u));
}

std::size_t ClampWindowLower(std::size_t center, std::size_t window) {
  if (center <= kFirstGroupOffset) {
    return kFirstGroupOffset;
  }
  const std::size_t lower =
      (center > window) ? (center - window) : kFirstGroupOffset;
  return std::max(lower, kFirstGroupOffset);
}

uint32_t SafeFrameAdvance(uint32_t frame, uint32_t delta) {
  const uint64_t end = static_cast<uint64_t>(frame) + static_cast<uint64_t>(delta);
  return end > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())
             ? std::numeric_limits<uint32_t>::max()
             : static_cast<uint32_t>(end);
}

bool HasGroupStartCodeAt(const uint8_t* base,
                         std::size_t size,
                         std::size_t offset) {
  return base != NULL &&
         offset <= size &&
         size - offset >= sizeof(uint32_t) &&
         ReadLe32(base + offset) == kGroupStartCode;
}

bool FindPreviousGroupStartCode(const uint8_t* base,
                                std::size_t size,
                                std::size_t start_offset,
                                std::size_t lower_bound,
                                std::size_t* out_offset) {
  if (out_offset == NULL || start_offset < lower_bound) {
    return false;
  }

  for (std::size_t candidate = AlignDown4(start_offset);; candidate -= 4u) {
    if (HasGroupStartCodeAt(base, size, candidate)) {
      *out_offset = candidate;
      return true;
    }
    if (candidate <= lower_bound) {
      break;
    }
  }

  return false;
}

bool ParseGroupAt(const uint8_t* base,
                  std::size_t size,
                  std::size_t group_offset,
                  ParsedGroup* out) {
  if (out == NULL) {
    return false;
  }

  ParsedGroup parsed;
  if (!ParseGroupHeader(base, size, group_offset, &parsed.header)) {
    return false;
  }
  if (parsed.header.block_count == 0u ||
      parsed.header.block_count > kMaxReasonableBlockCount) {
    return false;
  }

  parsed.group_offset = group_offset;
  std::size_t cursor = group_offset + kGroupHeaderSize;
  for (uint32_t index = 0u; index < parsed.header.block_count; ++index) {
    BlockHeaderView block_header;
    if (!ParseBlockHeader(base, size, cursor, &block_header)) {
      return false;
    }
    if (IsVideoBlockType(block_header.block_type)) {
      ++parsed.video_block_count;
    }
    cursor += kBlockHeaderSize + block_header.block_size;
    if (cursor > size) {
      return false;
    }
  }

  parsed.next_group_offset = cursor;
  parsed.has_next_group =
      (cursor + kGroupHeaderSize <= size) &&
      ParseGroupHeader(base, size, cursor, &parsed.next_group_header);

  *out = parsed;
  return true;
}

bool FindNextNonAudioGroup(const uint8_t* base,
                           std::size_t size,
                           ParsedGroup* group) {
  if (group == NULL) {
    return false;
  }

  std::size_t cursor = group->next_group_offset;
  while (cursor + kGroupHeaderSize <= size) {
    ParsedGroup next_group;
    if (!ParseGroupAt(base, size, cursor, &next_group)) {
      return false;
    }
    if (!next_group.header.is_audio) {
      group->has_next_non_audio = true;
      group->next_non_audio_group_offset = next_group.group_offset;
      group->next_non_audio_header = next_group.header;
      return true;
    }
    cursor = next_group.next_group_offset;
  }

  group->has_next_non_audio = false;
  return false;
}

uint32_t ComputeGroupFrameSpan(const ParsedGroup& group) {
  if (group.header.is_audio) {
    return 0u;
  }

  if (group.has_next_non_audio &&
      group.next_non_audio_header.frame_num > group.header.frame_num) {
    return group.next_non_audio_header.frame_num - group.header.frame_num;
  }

  if (group.video_block_count > 0u) {
    return group.video_block_count;
  }

  return 1u;
}

bool HasImmediateSharedFrame(const ParsedGroup& group) {
  return group.has_next_group &&
         !group.header.is_audio &&
         !group.next_group_header.is_audio &&
         group.next_group_header.frame_num == group.header.frame_num;
}

bool GroupContainsFrame(const ParsedGroup& group, uint32_t target_frame) {
  if (group.header.is_audio) {
    return false;
  }
  if (target_frame < group.header.frame_num) {
    return false;
  }
  if (HasImmediateSharedFrame(group)) {
    return target_frame == group.header.frame_num;
  }

  const uint32_t frame_span = ComputeGroupFrameSpan(group);
  const uint32_t frame_end = SafeFrameAdvance(group.header.frame_num, frame_span);
  return target_frame < frame_end;
}

bool AppendMatch(const ParsedGroup& parsed_group,
                 uint32_t target_frame,
                 uint32_t block_index_in_group,
                 std::size_t block_header_offset,
                 bool continuation,
                 SearchMatches* out) {
  if (out == NULL || out->count >= 2u) {
    return false;
  }

  SearchResult result;
  result.target_frame = target_frame;
  result.group_start_frame = parsed_group.header.frame_num;
  result.next_group_start_frame =
      parsed_group.has_next_non_audio ? parsed_group.next_non_audio_header.frame_num
                                      : SafeFrameAdvance(parsed_group.header.frame_num,
                                                         ComputeGroupFrameSpan(parsed_group));
  result.group_frame_span = ComputeGroupFrameSpan(parsed_group);
  result.block_index_in_group = block_index_in_group;
  result.group_file_offset = parsed_group.group_offset;
  result.block_header_offset = block_header_offset;
  result.block_data_offset = block_header_offset + kBlockHeaderSize;
  result.found = true;
  result.continuation = continuation;

  out->items[out->count] = result;
  ++out->count;
  return true;
}

bool AppendFrameMatchFromGroup(const uint8_t* base,
                               std::size_t size,
                               const ParsedGroup& parsed_group,
                               uint32_t target_frame,
                               bool continuation,
                               SearchMatches* out) {
  if (!GroupContainsFrame(parsed_group, target_frame)) {
    return false;
  }

  uint32_t desired_video_index = 0u;
  const uint32_t frame_span = ComputeGroupFrameSpan(parsed_group);
  if (frame_span > 1u &&
      target_frame >= parsed_group.header.frame_num &&
      parsed_group.video_block_count > 1u) {
    desired_video_index = target_frame - parsed_group.header.frame_num;
    if (desired_video_index >= parsed_group.video_block_count) {
      desired_video_index = parsed_group.video_block_count - 1u;
    }
  }

  std::size_t cursor = parsed_group.group_offset + kGroupHeaderSize;
  uint32_t video_index = 0u;
  for (uint32_t block_index = 0u; block_index < parsed_group.header.block_count;
       ++block_index) {
    BlockHeaderView block_header;
    if (!ParseBlockHeader(base, size, cursor, &block_header)) {
      return false;
    }

    if (IsVideoBlockType(block_header.block_type)) {
      if (video_index == desired_video_index) {
        return AppendMatch(parsed_group,
                           target_frame,
                           block_index,
                           cursor,
                           continuation,
                           out);
      }
      ++video_index;
    }

    cursor += kBlockHeaderSize + block_header.block_size;
  }

  return false;
}

bool FindPreviousGroupEndingAt(const uint8_t* base,
                               std::size_t size,
                               std::size_t end_offset,
                               std::size_t max_scan_bytes,
                               ParsedGroup* out) {
  if (out == NULL || end_offset <= kFirstGroupOffset) {
    return false;
  }

  const std::size_t lower = ClampWindowLower(end_offset, max_scan_bytes);
  std::size_t candidate = 0u;
  std::size_t search_from = end_offset - 4u;
  while (FindPreviousGroupStartCode(base, size, search_from, lower, &candidate)) {
    ParsedGroup previous;
    if (ParseGroupAt(base, size, candidate, &previous) &&
        previous.next_group_offset == end_offset) {
      *out = previous;
      return true;
    }
    if (candidate <= lower) {
      break;
    }
    search_from = candidate - 4u;
  }

  return false;
}

bool FindContainingGroup(const uint8_t* base,
                         std::size_t size,
                         std::size_t offset,
                         std::size_t window_bytes,
                         ParsedGroup* out) {
  if (out == NULL || offset < kFirstGroupOffset || offset >= size) {
    return false;
  }

  const std::size_t lower = ClampWindowLower(offset, window_bytes);
  std::size_t candidate = 0u;
  std::size_t search_from = offset;
  while (FindPreviousGroupStartCode(base, size, search_from, lower, &candidate)) {
    ParsedGroup parsed_group;
    if (ParseGroupAt(base, size, candidate, &parsed_group) &&
        parsed_group.group_offset <= offset &&
        offset < parsed_group.next_group_offset) {
      *out = parsed_group;
      return true;
    }
    if (candidate <= lower) {
      break;
    }
    search_from = candidate - 4u;
  }

  return false;
}

}  // namespace detail
}  // namespace private_mp4
