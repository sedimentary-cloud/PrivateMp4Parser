#include "frame_locator.h"

#include "mp4_format.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "gtest/gtest.h"

namespace private_mp4 {
namespace {

struct GroupSpec {
  uint32_t frame_num;
  bool is_audio;
  std::vector<uint16_t> block_types;
};

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
 * @brief AppendGroup writes one synthetic group with small fixed payload blocks.
 * @param spec Group description used to encode the group.
 * @param bytes Target byte buffer.
 * @return None.
 * @throws None.
 */
void AppendGroup(const GroupSpec& spec, std::vector<uint8_t>* bytes) {
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
    const uint32_t payload_size = 4u;
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
      bytes->push_back(static_cast<uint8_t>(0x10u + i + j));
    }
  }
}

/**
 * @brief BuildTestFile constructs a full synthetic private MP4 byte buffer.
 * @param groups Sequence of groups to encode.
 * @return Encoded file bytes.
 * @throws None.
 */
std::vector<uint8_t> BuildTestFile(const std::vector<GroupSpec>& groups) {
  std::vector<uint8_t> bytes;
  bytes.reserve(1024u);
  AppendFileHeader(&bytes);
  for (std::size_t i = 0u; i < groups.size(); ++i) {
    AppendGroup(groups[i], &bytes);
  }
  return bytes;
}

TEST(FrameLocatorTest, ReturnsFirstBlockOffsetForFirstFrame) {
  const GroupSpec groups[] = {
      {0u, false, std::vector<uint16_t>(1u, kVideoIBlock)},
  };
  const std::vector<uint8_t> bytes =
      BuildTestFile(std::vector<GroupSpec>(groups, groups + 1u));

  SearchMatches matches;
  ASSERT_TRUE(LocateFrame(bytes.data(), bytes.size(), 0u, &matches));
  ASSERT_EQ(1u, matches.count);
  EXPECT_EQ(0x28u, matches.items[0].group_file_offset);
  EXPECT_EQ(0x6Cu, matches.items[0].block_data_offset);
}

TEST(FrameLocatorTest, SkipsAudioGroupsWhenCountingFrames) {
  const GroupSpec groups[] = {
      {0u, false, std::vector<uint16_t>(1u, kVideoIBlock)},
      {1u, true, std::vector<uint16_t>(1u, kAudioIBlock)},
      {1u, false, std::vector<uint16_t>(1u, kVideoPBlock)},
  };
  const std::vector<uint8_t> bytes =
      BuildTestFile(std::vector<GroupSpec>(groups, groups + 3u));

  SearchMatches matches;
  ASSERT_TRUE(LocateFrame(bytes.data(), bytes.size(), 1u, &matches));
  ASSERT_EQ(1u, matches.count);
  EXPECT_GT(matches.items[0].group_file_offset, 0x28u);
  EXPECT_EQ(1u, matches.items[0].group_start_frame);
}

TEST(FrameLocatorTest, MapsFrameRangeToNthVideoBlock) {
  const GroupSpec groups[] = {
      {10u, false, std::vector<uint16_t>(3u, kVideoPBlock)},
      {13u, false, std::vector<uint16_t>(1u, kVideoIBlock)},
  };
  const std::vector<uint8_t> bytes =
      BuildTestFile(std::vector<GroupSpec>(groups, groups + 2u));

  SearchMatches matches;
  ASSERT_TRUE(LocateFrame(bytes.data(), bytes.size(), 12u, &matches));
  ASSERT_EQ(1u, matches.count);
  EXPECT_EQ(10u, matches.items[0].group_start_frame);
  EXPECT_EQ(3u, matches.items[0].group_frame_span);
  EXPECT_EQ(2u, matches.items[0].block_index_in_group);
}

TEST(FrameLocatorTest, ReturnsTwoMatchesForSharedFrameAcrossGroups) {
  const GroupSpec groups[] = {
      {0u, false, std::vector<uint16_t>(1u, kVideoIBlock)},
      {0u, false, std::vector<uint16_t>(1u, kVideoPBlock)},
      {1u, false, std::vector<uint16_t>(1u, kVideoIBlock)},
  };
  const std::vector<uint8_t> bytes =
      BuildTestFile(std::vector<GroupSpec>(groups, groups + 3u));

  SearchMatches matches;
  ASSERT_TRUE(LocateFrame(bytes.data(), bytes.size(), 0u, &matches));
  ASSERT_EQ(2u, matches.count);
  EXPECT_EQ(0u, matches.items[0].group_start_frame);
  EXPECT_EQ(0u, matches.items[1].group_start_frame);
  EXPECT_FALSE(matches.items[0].continuation);
  EXPECT_TRUE(matches.items[1].continuation);
  EXPECT_LT(matches.items[0].group_file_offset, matches.items[1].group_file_offset);
}

TEST(FrameLocatorTest, ReturnsFalseForFrameBeyondLastGroup) {
  const GroupSpec groups[] = {
      {0u, false, std::vector<uint16_t>(1u, kVideoIBlock)},
      {1u, false, std::vector<uint16_t>(1u, kVideoPBlock)},
  };
  const std::vector<uint8_t> bytes =
      BuildTestFile(std::vector<GroupSpec>(groups, groups + 2u));

  SearchMatches matches;
  ASSERT_FALSE(LocateFrame(bytes.data(), bytes.size(), 5u, &matches));
  EXPECT_EQ(0u, matches.count);
}

TEST(FrameLocatorTest, ReturnsFalseForInvalidFileHeader) {
  const GroupSpec groups[] = {
      {0u, false, std::vector<uint16_t>(1u, kVideoIBlock)},
  };
  std::vector<uint8_t> bytes =
      BuildTestFile(std::vector<GroupSpec>(groups, groups + 1u));
  bytes[0] = 0x00u;
  bytes[1] = 0x00u;
  bytes[2] = 0x00u;
  bytes[3] = 0x00u;

  SearchMatches matches;
  ASSERT_FALSE(LocateFrame(bytes.data(), bytes.size(), 0u, &matches));
  EXPECT_EQ(0u, matches.count);
}

TEST(FrameLocatorTest, ReturnsFalseForTruncatedBlockPayload) {
  const GroupSpec groups[] = {
      {0u, false, std::vector<uint16_t>(1u, kVideoIBlock)},
      {1u, false, std::vector<uint16_t>(1u, kVideoPBlock)},
  };
  std::vector<uint8_t> bytes =
      BuildTestFile(std::vector<GroupSpec>(groups, groups + 2u));
  bytes.resize(kFileHeaderSize + kGroupHeaderSize + kBlockHeaderSize + 2u);

  SearchMatches matches;
  ASSERT_FALSE(LocateFrame(bytes.data(), bytes.size(), 0u, &matches));
  EXPECT_EQ(0u, matches.count);
}

TEST(FrameLocatorTest, ReturnsFalseForCorruptedBlockType) {
  const GroupSpec groups[] = {
      {0u, false, std::vector<uint16_t>(1u, kVideoIBlock)},
      {1u, false, std::vector<uint16_t>(1u, kVideoPBlock)},
  };
  std::vector<uint8_t> bytes =
      BuildTestFile(std::vector<GroupSpec>(groups, groups + 2u));
  const std::size_t first_block_header_offset = kFileHeaderSize + kGroupHeaderSize;
  bytes[first_block_header_offset + 0u] = 0xFFu;
  bytes[first_block_header_offset + 1u] = 0x7Fu;

  SearchMatches matches;
  ASSERT_FALSE(LocateFrame(bytes.data(), bytes.size(), 0u, &matches));
  EXPECT_EQ(0u, matches.count);
}

TEST(FrameLocatorTest, ReturnsFalseWhenOutputIsNull) {
  const GroupSpec groups[] = {
      {0u, false, std::vector<uint16_t>(1u, kVideoIBlock)},
  };
  const std::vector<uint8_t> bytes =
      BuildTestFile(std::vector<GroupSpec>(groups, groups + 1u));

  ASSERT_FALSE(LocateFrame(bytes.data(), bytes.size(), 0u, NULL));
}

TEST(FrameLocatorTest, ReturnsFalseForHeaderOnlyFile) {
  const GroupSpec groups[] = {
      {0u, false, std::vector<uint16_t>(1u, kVideoIBlock)},
  };
  std::vector<uint8_t> bytes =
      BuildTestFile(std::vector<GroupSpec>(groups, groups + 1u));
  bytes.resize(kFileHeaderSize);

  SearchMatches matches;
  ASSERT_FALSE(LocateFrame(bytes.data(), bytes.size(), 0u, &matches));
  EXPECT_EQ(0u, matches.count);
}

TEST(FrameLocatorTest, FallsBackToLinearSearchWhenProbeWindowIsTooSmall) {
  const GroupSpec groups[] = {
      {0u, false, std::vector<uint16_t>(1u, kVideoIBlock)},
      {1u, false, std::vector<uint16_t>(1u, kVideoPBlock)},
      {2u, false, std::vector<uint16_t>(1u, kVideoPBlock)},
      {3u, false, std::vector<uint16_t>(1u, kVideoPBlock)},
  };
  const std::vector<uint8_t> bytes =
      BuildTestFile(std::vector<GroupSpec>(groups, groups + 4u));

  SearchOptions options;
  options.probe_window_bytes = 1u;
  options.backward_scan_bytes = 1024u;

  SearchMatches matches;
  ASSERT_TRUE(LocateFrame(bytes.data(), bytes.size(), 2u, &matches, options));
  ASSERT_EQ(1u, matches.count);
  EXPECT_EQ(2u, matches.items[0].target_frame);
}

TEST(FrameLocatorTest, ReturnsFalseWhenTargetIsBeforeFirstFrame) {
  const GroupSpec groups[] = {
      {10u, false, std::vector<uint16_t>(1u, kVideoIBlock)},
      {11u, false, std::vector<uint16_t>(1u, kVideoPBlock)},
      {12u, false, std::vector<uint16_t>(1u, kVideoPBlock)},
  };
  const std::vector<uint8_t> bytes =
      BuildTestFile(std::vector<GroupSpec>(groups, groups + 3u));

  SearchMatches matches;
  ASSERT_FALSE(LocateFrame(bytes.data(), bytes.size(), 0u, &matches));
  EXPECT_EQ(0u, matches.count);
}

}  // namespace
}  // namespace private_mp4
