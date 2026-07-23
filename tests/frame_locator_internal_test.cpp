#include "frame_locator_internal.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "gtest/gtest.h"
#include "synthetic_mp4_support.h"

namespace private_mp4 {
namespace detail {
namespace {

/**
 * @brief MakeGroup constructs a synthetic group descriptor for tests.
 * @param frame_num Starting frame number for the group.
 * @param is_audio Whether the group should be treated as audio.
 * @param block_types Block type sequence stored in the group.
 * @return Synthetic group descriptor.
 * @throws None.
 */
SyntheticGroupSpec MakeGroup(uint32_t frame_num,
                             bool is_audio,
                             const std::vector<uint16_t>& block_types) {
  SyntheticGroupSpec group;
  group.frame_num = frame_num;
  group.is_audio = is_audio;
  group.block_types = block_types;
  group.payload_size = 4u;
  return group;
}

/**
 * @brief BuildBytes serializes a synthetic group list into file bytes.
 * @param groups Group sequence to encode.
 * @return Encoded file bytes.
 * @throws None.
 */
std::vector<uint8_t> BuildBytes(const std::vector<SyntheticGroupSpec>& groups) {
  std::vector<uint8_t> bytes;
  EXPECT_TRUE(BuildSyntheticFileBytes(groups, &bytes));
  return bytes;
}

TEST(FrameLocatorInternalTest, AlignDown4RoundsTowardLowerBoundary) {
  EXPECT_EQ(0u, AlignDown4(3u));
  EXPECT_EQ(4u, AlignDown4(7u));
  EXPECT_EQ(8u, AlignDown4(8u));
}

TEST(FrameLocatorInternalTest, ClampWindowLowerHonorsFileHeaderLowerBound) {
  EXPECT_EQ(kFileHeaderSize, ClampWindowLower(kFileHeaderSize, 1024u));
  EXPECT_EQ(kFileHeaderSize, ClampWindowLower(kFileHeaderSize + 4u, 1024u));
  EXPECT_EQ(100u, ClampWindowLower(120u, 20u));
}

TEST(FrameLocatorInternalTest, SafeFrameAdvanceSaturatesOnOverflow) {
  EXPECT_EQ(12u, SafeFrameAdvance(10u, 2u));
  EXPECT_EQ(0xFFFFFFFFu, SafeFrameAdvance(0xFFFFFFFEu, 10u));
}

TEST(FrameLocatorInternalTest, HasGroupStartCodeAtMatchesOnlyRealHeaders) {
  const std::vector<SyntheticGroupSpec> groups = {
      MakeGroup(0u, false, std::vector<uint16_t>(1u, kVideoIBlock)),
  };
  const std::vector<uint8_t> bytes = BuildBytes(groups);

  EXPECT_TRUE(HasGroupStartCodeAt(bytes.data(), bytes.size(), kFileHeaderSize));
  EXPECT_FALSE(HasGroupStartCodeAt(bytes.data(), bytes.size(), kFileHeaderSize + 4u));
  EXPECT_FALSE(HasGroupStartCodeAt(bytes.data(), bytes.size(), bytes.size()));
}

TEST(FrameLocatorInternalTest, FindPreviousGroupStartCodeFindsNearestCandidate) {
  const std::vector<SyntheticGroupSpec> groups = {
      MakeGroup(0u, false, std::vector<uint16_t>(1u, kVideoIBlock)),
      MakeGroup(1u, false, std::vector<uint16_t>(1u, kVideoPBlock)),
  };
  const std::vector<uint8_t> bytes = BuildBytes(groups);

  ParsedGroup first_group;
  ASSERT_TRUE(ParseGroupAt(bytes.data(), bytes.size(), kFileHeaderSize, &first_group));
  ParsedGroup second_group;
  ASSERT_TRUE(ParseGroupAt(bytes.data(), bytes.size(), first_group.next_group_offset, &second_group));

  std::size_t offset = 0u;
  ASSERT_TRUE(FindPreviousGroupStartCode(bytes.data(),
                                         bytes.size(),
                                         second_group.group_offset + 12u,
                                         kFileHeaderSize,
                                         &offset));
  EXPECT_EQ(second_group.group_offset, offset);
}

TEST(FrameLocatorInternalTest, FindPreviousGroupStartCodeReturnsFalseWithoutCandidate) {
  const std::vector<SyntheticGroupSpec> groups = {
      MakeGroup(0u, false, std::vector<uint16_t>(1u, kVideoIBlock)),
  };
  const std::vector<uint8_t> bytes = BuildBytes(groups);

  std::size_t offset = 0u;
  ASSERT_FALSE(FindPreviousGroupStartCode(bytes.data(),
                                          bytes.size(),
                                          kFileHeaderSize + 4u,
                                          kFileHeaderSize + 4u,
                                          &offset));
}

TEST(FrameLocatorInternalTest, ParseGroupAtRejectsNullOutput) {
  const std::vector<uint8_t> bytes = BuildBytes(
      std::vector<SyntheticGroupSpec>(1u, MakeGroup(0u, false,
                                                    std::vector<uint16_t>(1u, kVideoIBlock))));
  ASSERT_FALSE(ParseGroupAt(bytes.data(), bytes.size(), kFileHeaderSize, NULL));
}

TEST(FrameLocatorInternalTest, ParseGroupAtRejectsExcessiveBlockCount) {
  std::vector<uint8_t> bytes = BuildBytes(
      std::vector<SyntheticGroupSpec>(1u, MakeGroup(0u, false,
                                                    std::vector<uint16_t>(1u, kVideoIBlock))));
  const std::size_t block_count_offset = kFileHeaderSize + 16u;
  const uint32_t raw_block_count = kConstNumberBase + 5000u;
  bytes[block_count_offset + 0u] = static_cast<uint8_t>(raw_block_count & 0xFFu);
  bytes[block_count_offset + 1u] = static_cast<uint8_t>((raw_block_count >> 8) & 0xFFu);
  bytes[block_count_offset + 2u] = static_cast<uint8_t>((raw_block_count >> 16) & 0xFFu);
  bytes[block_count_offset + 3u] = static_cast<uint8_t>((raw_block_count >> 24) & 0xFFu);

  ParsedGroup group;
  ASSERT_FALSE(ParseGroupAt(bytes.data(), bytes.size(), kFileHeaderSize, &group));
}

TEST(FrameLocatorInternalTest, ParseGroupAtParsesValidGroupAndTracksNextGroup) {
  const std::vector<SyntheticGroupSpec> groups = {
      MakeGroup(0u, false, std::vector<uint16_t>(1u, kVideoIBlock)),
      MakeGroup(1u, false, std::vector<uint16_t>(1u, kVideoPBlock)),
  };
  const std::vector<uint8_t> bytes = BuildBytes(groups);

  ParsedGroup group;
  ASSERT_TRUE(ParseGroupAt(bytes.data(), bytes.size(), kFileHeaderSize, &group));
  EXPECT_EQ(0u, group.header.frame_num);
  EXPECT_TRUE(group.has_next_group);
  EXPECT_EQ(1u, group.next_group_header.frame_num);
}

TEST(FrameLocatorInternalTest, FindNextNonAudioGroupSkipsAudioAndFindsVideo) {
  const std::vector<SyntheticGroupSpec> groups = {
      MakeGroup(0u, false, std::vector<uint16_t>(1u, kVideoIBlock)),
      MakeGroup(1u, true, std::vector<uint16_t>(1u, kAudioIBlock)),
      MakeGroup(1u, false, std::vector<uint16_t>(1u, kVideoPBlock)),
  };
  const std::vector<uint8_t> bytes = BuildBytes(groups);

  ParsedGroup group;
  ASSERT_TRUE(ParseGroupAt(bytes.data(), bytes.size(), kFileHeaderSize, &group));
  ASSERT_TRUE(FindNextNonAudioGroup(bytes.data(), bytes.size(), &group));
  EXPECT_TRUE(group.has_next_non_audio);
  EXPECT_EQ(1u, group.next_non_audio_header.frame_num);
}

TEST(FrameLocatorInternalTest, FindNextNonAudioGroupReturnsFalseWhenOnlyAudioRemains) {
  const std::vector<SyntheticGroupSpec> groups = {
      MakeGroup(0u, false, std::vector<uint16_t>(1u, kVideoIBlock)),
      MakeGroup(1u, true, std::vector<uint16_t>(1u, kAudioIBlock)),
  };
  const std::vector<uint8_t> bytes = BuildBytes(groups);

  ParsedGroup group;
  ASSERT_TRUE(ParseGroupAt(bytes.data(), bytes.size(), kFileHeaderSize, &group));
  ASSERT_FALSE(FindNextNonAudioGroup(bytes.data(), bytes.size(), &group));
  EXPECT_FALSE(group.has_next_non_audio);
}

TEST(FrameLocatorInternalTest, ComputeGroupFrameSpanUsesNextNonAudioDelta) {
  const std::vector<SyntheticGroupSpec> groups = {
      MakeGroup(10u, false, std::vector<uint16_t>(3u, kVideoPBlock)),
      MakeGroup(13u, false, std::vector<uint16_t>(1u, kVideoIBlock)),
  };
  const std::vector<uint8_t> bytes = BuildBytes(groups);

  ParsedGroup group;
  ASSERT_TRUE(ParseGroupAt(bytes.data(), bytes.size(), kFileHeaderSize, &group));
  ASSERT_TRUE(FindNextNonAudioGroup(bytes.data(), bytes.size(), &group));
  EXPECT_EQ(3u, ComputeGroupFrameSpan(group));
}

TEST(FrameLocatorInternalTest, ComputeGroupFrameSpanFallsBackToVideoBlockCount) {
  ParsedGroup group;
  group.header.is_audio = false;
  group.video_block_count = 2u;
  group.has_next_non_audio = false;
  EXPECT_EQ(2u, ComputeGroupFrameSpan(group));
}

TEST(FrameLocatorInternalTest, ComputeGroupFrameSpanReturnsOneWhenNoVideoBlocks) {
  ParsedGroup group;
  group.header.is_audio = false;
  group.video_block_count = 0u;
  group.has_next_non_audio = false;
  EXPECT_EQ(1u, ComputeGroupFrameSpan(group));
}

TEST(FrameLocatorInternalTest, HasImmediateSharedFrameDetectsSharedFrame) {
  const std::vector<SyntheticGroupSpec> groups = {
      MakeGroup(0u, false, std::vector<uint16_t>(1u, kVideoIBlock)),
      MakeGroup(0u, false, std::vector<uint16_t>(1u, kVideoPBlock)),
  };
  const std::vector<uint8_t> bytes = BuildBytes(groups);

  ParsedGroup group;
  ASSERT_TRUE(ParseGroupAt(bytes.data(), bytes.size(), kFileHeaderSize, &group));
  EXPECT_TRUE(HasImmediateSharedFrame(group));
}

TEST(FrameLocatorInternalTest, GroupContainsFrameChecksBoundaries) {
  const std::vector<SyntheticGroupSpec> groups = {
      MakeGroup(10u, false, std::vector<uint16_t>(3u, kVideoPBlock)),
      MakeGroup(13u, false, std::vector<uint16_t>(1u, kVideoIBlock)),
  };
  const std::vector<uint8_t> bytes = BuildBytes(groups);

  ParsedGroup group;
  ASSERT_TRUE(ParseGroupAt(bytes.data(), bytes.size(), kFileHeaderSize, &group));
  ASSERT_TRUE(FindNextNonAudioGroup(bytes.data(), bytes.size(), &group));
  EXPECT_FALSE(GroupContainsFrame(group, 9u));
  EXPECT_TRUE(GroupContainsFrame(group, 10u));
  EXPECT_TRUE(GroupContainsFrame(group, 12u));
  EXPECT_FALSE(GroupContainsFrame(group, 13u));
}

TEST(FrameLocatorInternalTest, AppendMatchRejectsFullOutputArray) {
  ParsedGroup group;
  group.header.frame_num = 5u;
  group.header.is_audio = false;
  group.group_offset = 100u;
  group.video_block_count = 1u;

  SearchMatches matches;
  matches.count = 2u;
  ASSERT_FALSE(AppendMatch(group, 5u, 0u, 120u, false, &matches));
}

TEST(FrameLocatorInternalTest, AppendFrameMatchFromGroupReturnsFalseForNonMatchingFrame) {
  const std::vector<SyntheticGroupSpec> groups = {
      MakeGroup(0u, false, std::vector<uint16_t>(1u, kVideoIBlock)),
  };
  const std::vector<uint8_t> bytes = BuildBytes(groups);

  ParsedGroup group;
  ASSERT_TRUE(ParseGroupAt(bytes.data(), bytes.size(), kFileHeaderSize, &group));

  SearchMatches matches;
  ASSERT_FALSE(AppendFrameMatchFromGroup(bytes.data(), bytes.size(), group, 9u, false, &matches));
  EXPECT_EQ(0u, matches.count);
}

TEST(FrameLocatorInternalTest, AppendFrameMatchFromGroupReturnsFalseForAudioGroup) {
  const std::vector<SyntheticGroupSpec> groups = {
      MakeGroup(0u, true, std::vector<uint16_t>(1u, kAudioIBlock)),
  };
  const std::vector<uint8_t> bytes = BuildBytes(groups);

  ParsedGroup group;
  ASSERT_TRUE(ParseGroupAt(bytes.data(), bytes.size(), kFileHeaderSize, &group));

  SearchMatches matches;
  ASSERT_FALSE(AppendFrameMatchFromGroup(bytes.data(), bytes.size(), group, 0u, false, &matches));
  EXPECT_EQ(0u, matches.count);
}

TEST(FrameLocatorInternalTest, AppendFrameMatchFromGroupSelectsRequestedVideoBlock) {
  const std::vector<SyntheticGroupSpec> groups = {
      MakeGroup(10u, false, std::vector<uint16_t>(3u, kVideoPBlock)),
      MakeGroup(13u, false, std::vector<uint16_t>(1u, kVideoIBlock)),
  };
  const std::vector<uint8_t> bytes = BuildBytes(groups);

  ParsedGroup group;
  ASSERT_TRUE(ParseGroupAt(bytes.data(), bytes.size(), kFileHeaderSize, &group));
  ASSERT_TRUE(FindNextNonAudioGroup(bytes.data(), bytes.size(), &group));

  SearchMatches matches;
  ASSERT_TRUE(AppendFrameMatchFromGroup(bytes.data(), bytes.size(), group, 11u, true, &matches));
  ASSERT_EQ(1u, matches.count);
  EXPECT_EQ(1u, matches.items[0].block_index_in_group);
  EXPECT_TRUE(matches.items[0].continuation);
}

TEST(FrameLocatorInternalTest, FindPreviousGroupEndingAtFindsPreviousGroup) {
  const std::vector<SyntheticGroupSpec> groups = {
      MakeGroup(0u, false, std::vector<uint16_t>(1u, kVideoIBlock)),
      MakeGroup(1u, false, std::vector<uint16_t>(1u, kVideoPBlock)),
  };
  const std::vector<uint8_t> bytes = BuildBytes(groups);

  ParsedGroup second_group;
  ASSERT_TRUE(ParseGroupAt(bytes.data(), bytes.size(), kFileHeaderSize, &second_group));
  ASSERT_TRUE(ParseGroupAt(bytes.data(), bytes.size(), second_group.next_group_offset, &second_group));

  ParsedGroup previous_group;
  ASSERT_TRUE(FindPreviousGroupEndingAt(bytes.data(),
                                        bytes.size(),
                                        second_group.group_offset,
                                        4096u,
                                        &previous_group));
  EXPECT_EQ(0u, previous_group.header.frame_num);
}

TEST(FrameLocatorInternalTest, FindPreviousGroupEndingAtRejectsTooEarlyOffset) {
  ParsedGroup group;
  ASSERT_FALSE(FindPreviousGroupEndingAt(NULL, 0u, kFileHeaderSize, 1024u, &group));
}

TEST(FrameLocatorInternalTest, FindContainingGroupFindsContainingGroup) {
  const std::vector<SyntheticGroupSpec> groups = {
      MakeGroup(0u, false, std::vector<uint16_t>(2u, kVideoPBlock)),
      MakeGroup(2u, false, std::vector<uint16_t>(1u, kVideoIBlock)),
  };
  const std::vector<uint8_t> bytes = BuildBytes(groups);

  ParsedGroup first_group;
  ASSERT_TRUE(ParseGroupAt(bytes.data(), bytes.size(), kFileHeaderSize, &first_group));
  const std::size_t offset_inside_first_group = first_group.group_offset + kGroupHeaderSize + 1u;

  ParsedGroup containing_group;
  ASSERT_TRUE(FindContainingGroup(bytes.data(),
                                  bytes.size(),
                                  offset_inside_first_group,
                                  4096u,
                                  &containing_group));
  EXPECT_EQ(first_group.group_offset, containing_group.group_offset);
}

TEST(FrameLocatorInternalTest, FindContainingGroupRejectsOutOfRangeOffset) {
  ParsedGroup group;
  ASSERT_FALSE(FindContainingGroup(NULL, 0u, 0u, 1024u, &group));
}

TEST(FrameLocatorInternalTest, FindContainingGroupReturnsFalseWhenWindowTooSmall) {
  const std::vector<SyntheticGroupSpec> groups = {
      MakeGroup(0u, false, std::vector<uint16_t>(1u, kVideoIBlock)),
      MakeGroup(1u, false, std::vector<uint16_t>(1u, kVideoPBlock)),
  };
  const std::vector<uint8_t> bytes = BuildBytes(groups);

  ParsedGroup second_group;
  ASSERT_TRUE(ParseGroupAt(bytes.data(), bytes.size(), kFileHeaderSize, &second_group));
  ASSERT_TRUE(ParseGroupAt(bytes.data(), bytes.size(), second_group.next_group_offset, &second_group));

  ParsedGroup containing_group;
  ASSERT_FALSE(FindContainingGroup(bytes.data(),
                                   bytes.size(),
                                   second_group.group_offset + 5u,
                                   1u,
                                   &containing_group));
}

}  // namespace
}  // namespace detail
}  // namespace private_mp4
