#ifndef PRIVATE_MP4_MAPPED_FILE_WIN32_H_
#define PRIVATE_MP4_MAPPED_FILE_WIN32_H_

#include <cstddef>
#include <cstdint>
#include <string>

namespace private_mp4 {

/**
 * @brief MappedFileWin32 manages a read-only Windows file mapping.
 * @param None.
 * @return None.
 * @throws None. Errors are reported through return values and error_message().
 */
class MappedFileWin32 {
 public:
  /**
   * @brief MappedFileWin32 constructs an empty mapping wrapper.
   * @param None.
   * @return None.
   * @throws None.
   */
  MappedFileWin32();

  /**
   * @brief ~MappedFileWin32 releases all operating-system resources.
   * @param None.
   * @return None.
   * @throws None.
   */
  ~MappedFileWin32();

  /**
   * @brief Open creates a read-only mapping for a file path.
   * @param path UTF-8 or ANSI path string accepted by CreateFileA.
   * @return True when the mapping is created successfully.
   * @throws None.
   */
  bool Open(const std::string& path);

  /**
   * @brief Close releases the active mapping, if any.
   * @param None.
   * @return None.
   * @throws None.
   */
  void Close();

  /**
   * @brief data returns the mapped base pointer.
   * @param None.
   * @return Pointer to the first mapped byte, or NULL when closed.
   * @throws None.
   */
  const uint8_t* data() const;

  /**
   * @brief size returns the mapped file size in bytes.
   * @param None.
   * @return Number of mapped bytes.
   * @throws None.
   */
  std::size_t size() const;

  /**
   * @brief is_open reports whether a file is currently mapped.
   * @param None.
   * @return True when the mapping is active.
   * @throws None.
   */
  bool is_open() const;

  /**
   * @brief error_message returns the last open failure description.
   * @param None.
   * @return Human-readable error string.
   * @throws None.
   */
  const std::string& error_message() const;

 private:
  void* file_handle_;
  void* mapping_handle_;
  const uint8_t* data_;
  std::size_t size_;
  std::string error_message_;
};

}  // namespace private_mp4

#endif  // PRIVATE_MP4_MAPPED_FILE_WIN32_H_
