#include "synthetic_mp4_support.h"

#include "mp4_format.h"

#include <fstream>
#include <limits>

namespace private_mp4 {

namespace {

/**
 * @brief WriteLe16 appends a 16-bit little-endian value to a byte vector.
 * @param value Value to encode.
 * @param bytes Target byte buffer.
 * @return None.
 * @throws None.
 */
void WriteLe16(uint16_t value, std::vector<uint8_t>* bytes) {
  bytes->push_back(static_cast<uint8_t>(value & 0xFFu));
  bytes->push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
}

/**
 * @brief WriteLe32 appends a 32-bit little-endian value to a byte vector.
 * @param value Value to encode.
 * @param bytes Target byte buffer.
 * @return None.
 * @throws None.
 */
void WriteLe32(uint32_t value, std::vector<uint8_t>* bytes) {
  bytes->push_back(static_cast<uint8_t>(value & 0xFFu));
  bytes->push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
  bytes->push_back(static_cast<uint8_t>((value >> 16) & 0xFFu));
  bytes->push_back(static_cast<uint8_t>((value >> 24) & 0xFFu));
}

/**
 * @brief NextRandom advances a lightweight deterministic pseudo-random state.
 * @param state Mutable PRNG state.
 * @return Next pseudo-random unsigned integer.
 * @throws None.
 */
uint32_t NextRandom(uint32_t* state) {
  *state = (*state * 1664525u) + 1013904223u;
  return *state;
}

/**
 * @brief ResolveBlockPayloadSize picks a deterministic payload size for one block.
 * @param spec Group description that may contain per-block payload overrides.
 * @param block_index Zero-based block index inside the group.
 * @return Payload size in bytes for the selected block.
 * @throws None.
 */
uint32_t ResolveBlockPayloadSize(const SyntheticGroupSpec& spec,
                                 std::size_t block_index) {
  if (block_index < spec.block_payload_sizes.size()) {
    return spec.block_payload_sizes[block_index];
  }
  return spec.payload_size;
}

/**
 * @brief AppendFileHeader writes a minimal valid file header.
 * @param bytes Target byte buffer.
 * @return None.
 * @throws None.
 */
void AppendFileHeader(std::vector<uint8_t>* bytes) {
  WriteLe32(kStartCodeHkh4, bytes);
  WriteLe32(0xD6D0B3FEu, bytes);
  WriteLe32(0x20040308u, bytes);
  WriteLe32(0u, bytes);
  WriteLe16(0x1003u, bytes);
  WriteLe16(0x1001u, bytes);
  WriteLe16(0x1001u, bytes);
  WriteLe16(16u, bytes);
  WriteLe32(8000u, bytes);
  WriteLe32(0x00001001u, bytes);
  WriteLe32(0x00001011u, bytes);
  WriteLe32(0x00000004u, bytes);
}

/**
 * @brief AppendGroup writes one synthetic group and its block payloads.
 * @param spec Group description used to encode the group.
 * @param bytes Target byte buffer.
 * @return None.
 * @throws None.
 */
void AppendGroup(const SyntheticGroupSpec& spec, std::vector<uint8_t>* bytes) {
  WriteLe32(kGroupStartCode, bytes);
  WriteLe32(kConstNumberBase + spec.frame_num, bytes);
  WriteLe32(0u, bytes);
  WriteLe32(kConstNumberBase + (spec.is_audio ? 1u : 0u), bytes);
  WriteLe32(kConstNumberBase + static_cast<uint32_t>(spec.block_types.size()), bytes);
  WriteLe32(0x00001001u, bytes);
  WriteLe32(0x00001001u, bytes);
  WriteLe32(25u, bytes);
  WriteLe32(0u, bytes);
  WriteLe32(0u, bytes);
  WriteLe32(0u, bytes);
  WriteLe32(0u, bytes);

  for (std::size_t i = 0u; i < spec.block_types.size(); ++i) {
    const uint32_t payload_size = ResolveBlockPayloadSize(spec, i);
    WriteLe16(spec.block_types[i], bytes);
    WriteLe16(1u, bytes);
    WriteLe32(0u, bytes);
    WriteLe32(0u, bytes);
    bytes->push_back(0);
    bytes->push_back(0);
    bytes->push_back(0);
    bytes->push_back(0);
    WriteLe32(payload_size, bytes);
    for (uint32_t j = 0u; j < payload_size; ++j) {
      bytes->push_back(static_cast<uint8_t>((i + j + spec.frame_num) & 0xFFu));
    }
  }
}

}  // namespace

bool BuildSyntheticGroupPlan(const SyntheticFileConfig& config,
                             std::vector<SyntheticGroupSpec>* groups) {
  if (groups == NULL || config.target_video_frames == 0u ||
      config.min_payload_size_bytes == 0u ||
      config.max_payload_size_bytes == 0u ||
      config.min_payload_size_bytes > config.max_payload_size_bytes) {
    return false;
  }

  groups->clear();
  groups->reserve(static_cast<std::size_t>(config.target_video_frames) * 2u);

  uint32_t current_frame = 0u;
  uint32_t produced_video_frames = 0u;
  uint32_t random_state = config.seed;
  uint32_t audio_counter = 0u;

  while (produced_video_frames < config.target_video_frames) {
    if (produced_video_frames < config.shared_frame_pair_count) {
      SyntheticGroupSpec first;
      first.frame_num = current_frame;
      first.is_audio = false;
      first.block_types.assign(1u, kVideoIBlock);
      first.payload_size = config.min_payload_size_bytes;
      first.block_payload_sizes.assign(
          1u,
          config.min_payload_size_bytes +
              (NextRandom(&random_state) %
               (config.max_payload_size_bytes - config.min_payload_size_bytes + 1u)));
      groups->push_back(first);

      SyntheticGroupSpec second;
      second.frame_num = current_frame;
      second.is_audio = false;
      second.block_types.assign(1u, kVideoPBlock);
      second.payload_size = config.min_payload_size_bytes;
      second.block_payload_sizes.assign(
          1u,
          config.min_payload_size_bytes +
              (NextRandom(&random_state) %
               (config.max_payload_size_bytes - config.min_payload_size_bytes + 1u)));
      groups->push_back(second);

      ++current_frame;
      ++produced_video_frames;
      audio_counter += (config.audio_group_period > 0u) ? 1u : 0u;
      continue;
    }

    if (config.audio_group_period > 0u &&
        audio_counter >= config.audio_group_period) {
      SyntheticGroupSpec audio_group;
      audio_group.frame_num = current_frame;
      audio_group.is_audio = true;
      audio_group.block_types.assign(1u, kAudioIBlock);
      audio_group.payload_size = config.min_payload_size_bytes / 2u;
      if (audio_group.payload_size == 0u) {
        audio_group.payload_size = 1u;
      }
      audio_group.block_payload_sizes.assign(1u, audio_group.payload_size);
      groups->push_back(audio_group);
      audio_counter = 0u;
      continue;
    }

    audio_counter += (config.audio_group_period > 0u) ? 1u : 0u;
    const uint32_t random_value = NextRandom(&random_state);
    uint32_t frame_span = 1u + (random_value % 3u);
    if (frame_span > config.target_video_frames - produced_video_frames) {
      frame_span = config.target_video_frames - produced_video_frames;
    }

    SyntheticGroupSpec group;
    group.frame_num = current_frame;
    group.is_audio = false;
    group.payload_size = config.min_payload_size_bytes;
    group.block_types.reserve(frame_span);
    group.block_payload_sizes.reserve(frame_span);
    for (uint32_t i = 0u; i < frame_span; ++i) {
      group.block_types.push_back((i == 0u) ? kVideoIBlock : kVideoPBlock);
      group.block_payload_sizes.push_back(
          config.min_payload_size_bytes +
          (NextRandom(&random_state) %
           (config.max_payload_size_bytes - config.min_payload_size_bytes + 1u)));
    }
    groups->push_back(group);

    current_frame += frame_span;
    produced_video_frames += frame_span;
    audio_counter += (config.audio_group_period > 0u) ? 1u : 0u;
  }

  return true;
}

bool BuildSyntheticFileBytes(const std::vector<SyntheticGroupSpec>& groups,
                             std::vector<uint8_t>* bytes) {
  if (bytes == NULL) {
    return false;
  }

  bytes->clear();
  bytes->reserve(groups.size() * 256u);
  AppendFileHeader(bytes);
  for (std::size_t i = 0u; i < groups.size(); ++i) {
    if (groups[i].block_types.empty()) {
      return false;
    }
    if (!groups[i].block_payload_sizes.empty() &&
        groups[i].block_payload_sizes.size() != groups[i].block_types.size()) {
      return false;
    }
    AppendGroup(groups[i], bytes);
  }
  return true;
}

bool WriteSyntheticFile(const std::string& path,
                        const SyntheticFileConfig& config,
                        std::size_t* file_size_bytes) {
  std::vector<SyntheticGroupSpec> groups;
  if (!BuildSyntheticGroupPlan(config, &groups)) {
    return false;
  }

  std::vector<uint8_t> bytes;
  if (!BuildSyntheticFileBytes(groups, &bytes)) {
    return false;
  }

  std::ofstream output(path.c_str(), std::ios::binary | std::ios::trunc);
  if (!output.good()) {
    return false;
  }

  output.write(reinterpret_cast<const char*>(&bytes[0]),
               static_cast<std::streamsize>(bytes.size()));
  if (!output.good()) {
    return false;
  }

  if (file_size_bytes != NULL) {
    *file_size_bytes = bytes.size();
  }
  return true;
}

}  // namespace private_mp4
