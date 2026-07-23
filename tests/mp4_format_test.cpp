#include "mp4_format.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "gtest/gtest.h"
#include "synthetic_mp4_support.h"

namespace private_mp4 {
namespace {

/**
 * @brief BuildSingleGroupFileBytes creates a minimal valid file with one group.
 * @param is_audio Whether the single group should be audio.
 * @return Encoded file bytes.
 * @throws None.
 */
std::vector<uint8_t> BuildSingleGroupFileBytes(bool is_audio) {
  SyntheticGroupSpec group;
  group.frame_num = 7u;
  group.is_audio = is_audio;
  group.block_types.assign(1u, is_audio ? kAudioIBlock : kVideoIBlock);
  group.payload_size = 4u;

  std::vector<SyntheticGroupSpec> groups(1u, group);
  std::vector<uint8_t> bytes;
  EXPECT_TRUE(BuildSyntheticFileBytes(groups, &bytes));
  return bytes;
}

TEST(Mp4FormatTest, ParsesValidFileHeader) {
  const std::vector<uint8_t> bytes = BuildSingleGroupFileBytes(false);
  FileHeaderView header;
  ASSERT_TRUE(ParseFileHeader(bytes.data(), bytes.size(), &header));
  EXPECT_EQ(kStartCodeHkh4, header.start_code);
}

TEST(Mp4FormatTest, RejectsInvalidFileHeaderStartCode) {
  std::vector<uint8_t> bytes = BuildSingleGroupFileBytes(false);
  bytes[0] = 0u;
  bytes[1] = 0u;
  bytes[2] = 0u;
  bytes[3] = 0u;
  FileHeaderView header;
  ASSERT_FALSE(ParseFileHeader(bytes.data(), bytes.size(), &header));
}

TEST(Mp4FormatTest, RejectsTruncatedFileHeader) {
  const std::vector<uint8_t> bytes = BuildSingleGroupFileBytes(false);
  FileHeaderView header;
  ASSERT_FALSE(ParseFileHeader(bytes.data(), kFileHeaderSize - 1u, &header));
}

TEST(Mp4FormatTest, ParsesValidGroupHeader) {
  const std::vector<uint8_t> bytes = BuildSingleGroupFileBytes(false);
  GroupHeaderView header;
  ASSERT_TRUE(ParseGroupHeader(bytes.data(), bytes.size(), kFileHeaderSize, &header));
  EXPECT_EQ(7u, header.frame_num);
  EXPECT_FALSE(header.is_audio);
  EXPECT_EQ(1u, header.block_count);
}

TEST(Mp4FormatTest, ParsesAudioGroupHeader) {
  const std::vector<uint8_t> bytes = BuildSingleGroupFileBytes(true);
  GroupHeaderView header;
  ASSERT_TRUE(ParseGroupHeader(bytes.data(), bytes.size(), kFileHeaderSize, &header));
  EXPECT_TRUE(header.is_audio);
}

TEST(Mp4FormatTest, RejectsGroupHeaderWithBadPictureMode) {
  std::vector<uint8_t> bytes = BuildSingleGroupFileBytes(false);
  const std::size_t picture_mode_offset = kFileHeaderSize + 24u;
  bytes[picture_mode_offset + 0u] = 0u;
  bytes[picture_mode_offset + 1u] = 0u;
  bytes[picture_mode_offset + 2u] = 0u;
  bytes[picture_mode_offset + 3u] = 0u;

  GroupHeaderView header;
  ASSERT_FALSE(ParseGroupHeader(bytes.data(), bytes.size(), kFileHeaderSize, &header));
}

TEST(Mp4FormatTest, RejectsGroupHeaderWithZeroFrameRate) {
  std::vector<uint8_t> bytes = BuildSingleGroupFileBytes(false);
  const std::size_t frame_rate_offset = kFileHeaderSize + 28u;
  bytes[frame_rate_offset + 0u] = 0u;
  bytes[frame_rate_offset + 1u] = 0u;
  bytes[frame_rate_offset + 2u] = 0u;
  bytes[frame_rate_offset + 3u] = 0u;

  GroupHeaderView header;
  ASSERT_FALSE(ParseGroupHeader(bytes.data(), bytes.size(), kFileHeaderSize, &header));
}

TEST(Mp4FormatTest, RejectsGroupHeaderWithInvalidBlockCountBase) {
  std::vector<uint8_t> bytes = BuildSingleGroupFileBytes(false);
  const std::size_t block_count_offset = kFileHeaderSize + 16u;
  bytes[block_count_offset + 0u] = 0u;
  bytes[block_count_offset + 1u] = 0u;
  bytes[block_count_offset + 2u] = 0u;
  bytes[block_count_offset + 3u] = 0u;

  GroupHeaderView header;
  ASSERT_FALSE(ParseGroupHeader(bytes.data(), bytes.size(), kFileHeaderSize, &header));
}

TEST(Mp4FormatTest, ParsesValidBlockHeader) {
  const std::vector<uint8_t> bytes = BuildSingleGroupFileBytes(false);
  BlockHeaderView header;
  const std::size_t block_offset = kFileHeaderSize + kGroupHeaderSize;
  ASSERT_TRUE(ParseBlockHeader(bytes.data(), bytes.size(), block_offset, &header));
  EXPECT_EQ(kVideoIBlock, header.block_type);
  EXPECT_EQ(4u, header.block_size);
}

TEST(Mp4FormatTest, RejectsInvalidBlockType) {
  std::vector<uint8_t> bytes = BuildSingleGroupFileBytes(false);
  const std::size_t block_offset = kFileHeaderSize + kGroupHeaderSize;
  bytes[block_offset + 0u] = 0xFFu;
  bytes[block_offset + 1u] = 0x7Fu;

  BlockHeaderView header;
  ASSERT_FALSE(ParseBlockHeader(bytes.data(), bytes.size(), block_offset, &header));
}

TEST(Mp4FormatTest, RejectsOversizedBlockPayload) {
  std::vector<uint8_t> bytes = BuildSingleGroupFileBytes(false);
  const std::size_t block_size_offset = kFileHeaderSize + kGroupHeaderSize + 16u;
  bytes[block_size_offset + 0u] = 0xFFu;
  bytes[block_size_offset + 1u] = 0xFFu;
  bytes[block_size_offset + 2u] = 0xFFu;
  bytes[block_size_offset + 3u] = 0x7Fu;

  BlockHeaderView header;
  ASSERT_FALSE(ParseBlockHeader(bytes.data(),
                                bytes.size(),
                                kFileHeaderSize + kGroupHeaderSize,
                                &header));
}

}  // namespace
}  // namespace private_mp4
