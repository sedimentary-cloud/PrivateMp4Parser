#include "mapped_file_win32.h"

#include <cstdio>
#include <fstream>
#include <string>

#include "gtest/gtest.h"

namespace private_mp4 {
namespace {

/**
 * @brief MakeTemporaryPath creates a deterministic test file path in the working directory.
 * @param file_name File name suffix to use.
 * @return Full relative path to the temporary test file.
 * @throws None.
 */
std::string MakeTemporaryPath(const std::string& file_name) {
  return file_name;
}

TEST(MappedFileWin32Test, FailsToOpenMissingFile) {
  MappedFileWin32 mapped_file;
  ASSERT_FALSE(mapped_file.Open("does_not_exist_private_mp4.bin"));
  EXPECT_FALSE(mapped_file.error_message().empty());
  EXPECT_FALSE(mapped_file.is_open());
}

TEST(MappedFileWin32Test, OpensAndReadsTemporaryFile) {
  const std::string path = MakeTemporaryPath("mapped_file_test.bin");
  {
    std::ofstream output(path.c_str(), std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(output.good());
    const char payload[] = {'A', 'B', 'C', 'D'};
    output.write(payload, sizeof(payload));
    ASSERT_TRUE(output.good());
  }

  MappedFileWin32 mapped_file;
  ASSERT_TRUE(mapped_file.Open(path));
  ASSERT_TRUE(mapped_file.is_open());
  ASSERT_EQ(4u, mapped_file.size());
  ASSERT_NE(static_cast<const uint8_t*>(NULL), mapped_file.data());
  EXPECT_EQ('A', static_cast<char>(mapped_file.data()[0]));
  EXPECT_EQ('D', static_cast<char>(mapped_file.data()[3]));

  mapped_file.Close();
  EXPECT_FALSE(mapped_file.is_open());
  EXPECT_EQ(0u, mapped_file.size());

  std::remove(path.c_str());
}

TEST(MappedFileWin32Test, FailsToOpenEmptyFile) {
  const std::string path = MakeTemporaryPath("mapped_file_empty.bin");
  {
    std::ofstream output(path.c_str(), std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(output.good());
  }

  MappedFileWin32 mapped_file;
  ASSERT_FALSE(mapped_file.Open(path));
  EXPECT_FALSE(mapped_file.error_message().empty());
  EXPECT_FALSE(mapped_file.is_open());

  std::remove(path.c_str());
}

}  // namespace
}  // namespace private_mp4
