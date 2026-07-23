#include "synthetic_mp4_support.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "gtest/gtest.h"
#include "mp4_format.h"

namespace private_mp4 {
namespace {

TEST(SyntheticMp4SupportTest, RejectsZeroTargetFrames) {
  SyntheticFileConfig config;
  config.target_video_frames = 0u;
  std::vector<SyntheticGroupSpec> groups;
  ASSERT_FALSE(BuildSyntheticGroupPlan(config, &groups));
}

TEST(SyntheticMp4SupportTest, RejectsZeroPayloadSize) {
  SyntheticFileConfig config;
  config.min_payload_size_bytes = 0u;
  std::vector<SyntheticGroupSpec> groups;
  ASSERT_FALSE(BuildSyntheticGroupPlan(config, &groups));
}

TEST(SyntheticMp4SupportTest, BuildsPlanWithSharedFramesAudioAndMultiBlockGroups) {
  SyntheticFileConfig config;
  config.target_video_frames = 32u;
  config.audio_group_period = 5u;
  config.shared_frame_pair_count = 2u;

  std::vector<SyntheticGroupSpec> groups;
  ASSERT_TRUE(BuildSyntheticGroupPlan(config, &groups));
  ASSERT_FALSE(groups.empty());

  EXPECT_EQ(0u, groups[0].frame_num);
  EXPECT_EQ(0u, groups[1].frame_num);
  EXPECT_FALSE(groups[0].is_audio);
  EXPECT_FALSE(groups[1].is_audio);

  bool saw_audio = false;
  bool saw_multi_block = false;
  for (std::size_t i = 0u; i < groups.size(); ++i) {
    saw_audio = saw_audio || groups[i].is_audio;
    saw_multi_block = saw_multi_block || (groups[i].block_types.size() > 1u);
  }
  EXPECT_TRUE(saw_audio);
  EXPECT_TRUE(saw_multi_block);
}

TEST(SyntheticMp4SupportTest, RejectsGroupWithNoBlocksWhenBuildingBytes) {
  SyntheticGroupSpec group;
  group.frame_num = 0u;
  group.is_audio = false;
  group.payload_size = 4u;

  std::vector<SyntheticGroupSpec> groups(1u, group);
  std::vector<uint8_t> bytes;
  ASSERT_FALSE(BuildSyntheticFileBytes(groups, &bytes));
}

TEST(SyntheticMp4SupportTest, BuildsValidFileBytes) {
  SyntheticFileConfig config;
  config.target_video_frames = 16u;

  std::vector<SyntheticGroupSpec> groups;
  ASSERT_TRUE(BuildSyntheticGroupPlan(config, &groups));

  std::vector<uint8_t> bytes;
  ASSERT_TRUE(BuildSyntheticFileBytes(groups, &bytes));
  ASSERT_GT(bytes.size(), kFileHeaderSize);

  FileHeaderView header;
  ASSERT_TRUE(ParseFileHeader(bytes.data(), bytes.size(), &header));
}

TEST(SyntheticMp4SupportTest, BuildsPlanWithoutAudioGroups) {
  SyntheticFileConfig config;
  config.target_video_frames = 8u;
  config.audio_group_period = 0u;
  config.shared_frame_pair_count = 0u;

  std::vector<SyntheticGroupSpec> groups;
  ASSERT_TRUE(BuildSyntheticGroupPlan(config, &groups));
  ASSERT_FALSE(groups.empty());

  for (std::size_t i = 0u; i < groups.size(); ++i) {
    EXPECT_FALSE(groups[i].is_audio);
  }
}

TEST(SyntheticMp4SupportTest, BuildsAudioGroupWithMinimumPayloadWhenHalfSizeIsZero) {
  SyntheticFileConfig config;
  config.target_video_frames = 4u;
  config.audio_group_period = 1u;
  config.shared_frame_pair_count = 0u;
  config.min_payload_size_bytes = 1u;
  config.max_payload_size_bytes = 1u;

  std::vector<SyntheticGroupSpec> groups;
  ASSERT_TRUE(BuildSyntheticGroupPlan(config, &groups));

  bool saw_audio = false;
  for (std::size_t i = 0u; i < groups.size(); ++i) {
    if (groups[i].is_audio) {
      saw_audio = true;
      EXPECT_EQ(1u, groups[i].payload_size);
    }
  }
  EXPECT_TRUE(saw_audio);
}

TEST(SyntheticMp4SupportTest, RejectsNullOutputWhenBuildingBytes) {
  const std::vector<SyntheticGroupSpec> groups(
      1u, SyntheticGroupSpec());
  ASSERT_FALSE(BuildSyntheticFileBytes(groups, NULL));
}

TEST(SyntheticMp4SupportTest, RejectsInvalidPayloadRange) {
  SyntheticFileConfig config;
  config.min_payload_size_bytes = 12288u;
  config.max_payload_size_bytes = 8192u;
  std::vector<SyntheticGroupSpec> groups;
  ASSERT_FALSE(BuildSyntheticGroupPlan(config, &groups));
}

TEST(SyntheticMp4SupportTest, GeneratesVideoPayloadsInsideConfiguredRange) {
  SyntheticFileConfig config;
  config.target_video_frames = 32u;
  config.min_payload_size_bytes = 8192u;
  config.max_payload_size_bytes = 12288u;

  std::vector<SyntheticGroupSpec> groups;
  ASSERT_TRUE(BuildSyntheticGroupPlan(config, &groups));

  bool saw_large_video_payload = false;
  for (std::size_t i = 0u; i < groups.size(); ++i) {
    if (groups[i].is_audio) {
      continue;
    }
    ASSERT_EQ(groups[i].block_types.size(), groups[i].block_payload_sizes.size());
    for (std::size_t j = 0u; j < groups[i].block_payload_sizes.size(); ++j) {
      EXPECT_GE(groups[i].block_payload_sizes[j], 8192u);
      EXPECT_LE(groups[i].block_payload_sizes[j], 12288u);
      saw_large_video_payload = saw_large_video_payload ||
                                (groups[i].block_payload_sizes[j] > 8192u);
    }
  }
  EXPECT_TRUE(saw_large_video_payload);
}

TEST(SyntheticMp4SupportTest, RejectsMismatchedBlockPayloadSizeVector) {
  SyntheticGroupSpec group;
  group.frame_num = 0u;
  group.is_audio = false;
  group.payload_size = 4u;
  group.block_types.assign(2u, kVideoPBlock);
  group.block_payload_sizes.assign(1u, 4u);

  std::vector<SyntheticGroupSpec> groups(1u, group);
  std::vector<uint8_t> bytes;
  ASSERT_FALSE(BuildSyntheticFileBytes(groups, &bytes));
}

TEST(SyntheticMp4SupportTest, WritesSyntheticFileAndReturnsSize) {
  const std::string path = "synthetic_write_test.mp4";
  SyntheticFileConfig config;
  config.target_video_frames = 12u;

  std::size_t file_size = 0u;
  ASSERT_TRUE(WriteSyntheticFile(path, config, &file_size));
  EXPECT_GT(file_size, 0u);

  std::remove(path.c_str());
}

TEST(SyntheticMp4SupportTest, RejectsInvalidConfigWhenWritingSyntheticFile) {
  SyntheticFileConfig config;
  config.target_video_frames = 0u;
  ASSERT_FALSE(WriteSyntheticFile("synthetic_invalid.mp4", config, NULL));
}

}  // namespace
}  // namespace private_mp4
