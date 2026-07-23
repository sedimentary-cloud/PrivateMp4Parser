#include "frame_query.h"

#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace private_mp4 {
namespace {

/**
 * @brief ExpectFrames asserts that a parsed frame vector matches the expected values.
 * @param actual Parsed frames.
 * @param expected Expected frames.
 * @return None.
 * @throws None.
 */
void ExpectFrames(const std::vector<uint32_t>& actual,
                  const std::vector<uint32_t>& expected) {
  ASSERT_EQ(expected.size(), actual.size());
  for (std::size_t i = 0u; i < expected.size(); ++i) {
    EXPECT_EQ(expected[i], actual[i]);
  }
}

TEST(FrameQueryTest, ParsesSingleFrame) {
  std::vector<uint32_t> frames;
  std::string error;
  ASSERT_TRUE(ParseFrameQuery("42", &frames, &error));
  ExpectFrames(frames, std::vector<uint32_t>(1u, 42u));
}

TEST(FrameQueryTest, ParsesMixedFramesAndRanges) {
  std::vector<uint32_t> frames;
  std::string error;
  ASSERT_TRUE(ParseFrameQuery("1, 5:7, 42", &frames, &error));
  const uint32_t expected_values[] = {1u, 5u, 6u, 7u, 42u};
  ExpectFrames(frames,
               std::vector<uint32_t>(expected_values,
                                     expected_values + sizeof(expected_values) /
                                                           sizeof(expected_values[0])));
}

TEST(FrameQueryTest, RejectsDescendingRanges) {
  std::vector<uint32_t> frames;
  std::string error;
  ASSERT_FALSE(ParseFrameQuery("10:2", &frames, &error));
  EXPECT_FALSE(error.empty());
}

TEST(FrameQueryTest, RejectsInvalidCharacters) {
  std::vector<uint32_t> frames;
  std::string error;
  ASSERT_FALSE(ParseFrameQuery("12,a", &frames, &error));
  EXPECT_FALSE(error.empty());
}

TEST(FrameQueryTest, RejectsEmptyQuery) {
  std::vector<uint32_t> frames;
  std::string error;
  ASSERT_FALSE(ParseFrameQuery("   ", &frames, &error));
  EXPECT_FALSE(error.empty());
}

TEST(FrameQueryTest, RejectsEmptyToken) {
  std::vector<uint32_t> frames;
  std::string error;
  ASSERT_FALSE(ParseFrameQuery("1,,3", &frames, &error));
  EXPECT_FALSE(error.empty());
}

TEST(FrameQueryTest, RejectsMultipleColons) {
  std::vector<uint32_t> frames;
  std::string error;
  ASSERT_FALSE(ParseFrameQuery("1:2:3", &frames, &error));
  EXPECT_FALSE(error.empty());
}

TEST(FrameQueryTest, RejectsOverflowValue) {
  std::vector<uint32_t> frames;
  std::string error;
  ASSERT_FALSE(ParseFrameQuery("4294967296", &frames, &error));
  EXPECT_FALSE(error.empty());
}

TEST(FrameQueryTest, ParsesMaxUint32Frame) {
  std::vector<uint32_t> frames;
  std::string error;
  ASSERT_TRUE(ParseFrameQuery("4294967295", &frames, &error));
  ExpectFrames(frames, std::vector<uint32_t>(1u, 0xFFFFFFFFu));
}

}  // namespace
}  // namespace private_mp4
