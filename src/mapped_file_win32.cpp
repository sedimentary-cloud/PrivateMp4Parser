#include "mapped_file_win32.h"

#include <windows.h>

#include <sstream>

namespace private_mp4 {

namespace {

/**
 * @brief BuildWindowsError formats a Win32 error code as text.
 * @param error_code Win32 error code returned by GetLastError().
 * @return Human-readable error text.
 * @throws None.
 */
std::string BuildWindowsError(DWORD error_code) {
  std::ostringstream stream;
  stream << "Win32 error " << static_cast<unsigned long>(error_code);
  return stream.str();
}

}  // namespace

MappedFileWin32::MappedFileWin32()
    : file_handle_(INVALID_HANDLE_VALUE),
      mapping_handle_(NULL),
      data_(NULL),
      size_(0u),
      error_message_() {}

MappedFileWin32::~MappedFileWin32() {
  Close();
}

bool MappedFileWin32::Open(const std::string& path) {
  Close();

  HANDLE file = CreateFileA(path.c_str(),
                            GENERIC_READ,
                            FILE_SHARE_READ,
                            NULL,
                            OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL,
                            NULL);
  if (file == INVALID_HANDLE_VALUE) {
    error_message_ = BuildWindowsError(GetLastError());
    return false;
  }

  LARGE_INTEGER file_size = {};
  if (GetFileSizeEx(file, &file_size) == 0 || file_size.QuadPart <= 0) {
    error_message_ = BuildWindowsError(GetLastError());
    CloseHandle(file);
    return false;
  }

  HANDLE mapping = CreateFileMappingA(file, NULL, PAGE_READONLY, 0, 0, NULL);
  if (mapping == NULL) {
    error_message_ = BuildWindowsError(GetLastError());
    CloseHandle(file);
    return false;
  }

  const uint8_t* data = static_cast<const uint8_t*>(
      MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
  if (data == NULL) {
    error_message_ = BuildWindowsError(GetLastError());
    CloseHandle(mapping);
    CloseHandle(file);
    return false;
  }

  file_handle_ = file;
  mapping_handle_ = mapping;
  data_ = data;
  size_ = static_cast<std::size_t>(file_size.QuadPart);
  error_message_.clear();
  return true;
}

void MappedFileWin32::Close() {
  if (data_ != NULL) {
    UnmapViewOfFile(data_);
    data_ = NULL;
  }
  if (mapping_handle_ != NULL) {
    CloseHandle(static_cast<HANDLE>(mapping_handle_));
    mapping_handle_ = NULL;
  }
  if (file_handle_ != INVALID_HANDLE_VALUE) {
    CloseHandle(static_cast<HANDLE>(file_handle_));
    file_handle_ = INVALID_HANDLE_VALUE;
  }
  size_ = 0u;
}

const uint8_t* MappedFileWin32::data() const {
  return data_;
}

std::size_t MappedFileWin32::size() const {
  return size_;
}

bool MappedFileWin32::is_open() const {
  return data_ != NULL;
}

const std::string& MappedFileWin32::error_message() const {
  return error_message_;
}

}  // namespace private_mp4
