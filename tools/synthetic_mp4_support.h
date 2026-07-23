#ifndef PRIVATE_MP4_SYNTHETIC_MP4_SUPPORT_H_
#define PRIVATE_MP4_SYNTHETIC_MP4_SUPPORT_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace private_mp4 {

struct SyntheticGroupSpec {
  uint32_t frame_num;
  bool is_audio;
  std::vector<uint16_t> block_types;
  std::vector<uint32_t> block_payload_sizes;
  uint32_t payload_size;
};

struct SyntheticFileConfig {
  SyntheticFileConfig()
      : target_video_frames(20000u),
        audio_group_period(11u),
        shared_frame_pair_count(2u),
        min_payload_size_bytes(48u),
        max_payload_size_bytes(48u),
        seed(12345u) {}

  uint32_t target_video_frames;
  uint32_t audio_group_period;
  uint32_t shared_frame_pair_count;
  uint32_t min_payload_size_bytes;
  uint32_t max_payload_size_bytes;
  uint32_t seed;
};

/**
 * @brief BuildSyntheticGroupPlan creates a mixed private-MP4 group sequence.
 * @param config Configuration for frame count, audio cadence and payload sizes.
 * @param groups Receives the generated group descriptors.
 * @return True when a valid plan is generated.
 * @throws None.
 */
bool BuildSyntheticGroupPlan(const SyntheticFileConfig& config,
                             std::vector<SyntheticGroupSpec>* groups);

/**
 * @brief BuildSyntheticFileBytes serializes a synthetic group plan into file bytes.
 * @param groups Group sequence to encode.
 * @param bytes Receives the complete file contents.
 * @return True when serialization succeeds.
 * @throws None.
 */
bool BuildSyntheticFileBytes(const std::vector<SyntheticGroupSpec>& groups,
                             std::vector<uint8_t>* bytes);

/**
 * @brief WriteSyntheticFile writes a synthetic private MP4 file to disk.
 * @param path Output file path.
 * @param config Configuration for file generation.
 * @param file_size_bytes Receives the generated file size.
 * @return True when the file is written successfully.
 * @throws None.
 */
bool WriteSyntheticFile(const std::string& path,
                        const SyntheticFileConfig& config,
                        std::size_t* file_size_bytes);

}  // namespace private_mp4

#endif  // PRIVATE_MP4_SYNTHETIC_MP4_SUPPORT_H_
