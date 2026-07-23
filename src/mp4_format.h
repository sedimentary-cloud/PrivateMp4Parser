#ifndef PRIVATE_MP4_FORMAT_H_
#define PRIVATE_MP4_FORMAT_H_

#include <cstddef>
#include <cstdint>

namespace private_mp4 {

static const std::size_t kFileHeaderSize = 40u;
static const std::size_t kGroupHeaderSize = 48u;
static const std::size_t kBlockHeaderSize = 20u;
static const uint32_t kConstNumberBase = 0x00001000u;
static const uint32_t kStartCodeHkm4 = 0x484B4D34u;
static const uint32_t kStartCodeHkh4 = 0x484B4834u;
static const uint32_t kGroupStartCode = 0x00000001u;

static const uint16_t kAudioIBlock = 0x1001u;
static const uint16_t kAudioPBlock = 0x1002u;
static const uint16_t kVideoIBlock = 0x1003u;
static const uint16_t kVideoPBlock = 0x1004u;
static const uint16_t kVideoBBlock = 0x1005u;

static const uint32_t kPictureModeMin = 0x00001001u;
static const uint32_t kPictureModeMax = 0x00001007u;

/**
 * @brief ReadLe16 reads a 16-bit little-endian unsigned value.
 * @param data Pointer to the first byte of the encoded value.
 * @return The decoded 16-bit value.
 * @throws None.
 */
inline uint16_t ReadLe16(const uint8_t* data) {
  return static_cast<uint16_t>(data[0]) |
         static_cast<uint16_t>(data[1] << 8);
}

/**
 * @brief ReadLe32 reads a 32-bit little-endian unsigned value.
 * @param data Pointer to the first byte of the encoded value.
 * @return The decoded 32-bit value.
 * @throws None.
 */
inline uint32_t ReadLe32(const uint8_t* data) {
  return static_cast<uint32_t>(data[0]) |
         (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) |
         (static_cast<uint32_t>(data[3]) << 24);
}

/**
 * @brief IsKnownBlockType checks whether a block type is part of the private format.
 * @param block_type Encoded block type.
 * @return True when the type is recognized.
 * @throws None.
 */
inline bool IsKnownBlockType(uint16_t block_type) {
  return block_type == kAudioIBlock ||
         block_type == kAudioPBlock ||
         block_type == kVideoIBlock ||
         block_type == kVideoPBlock ||
         block_type == kVideoBBlock;
}

/**
 * @brief IsVideoBlockType checks whether a block type belongs to video data.
 * @param block_type Encoded block type.
 * @return True when the block carries video.
 * @throws None.
 */
inline bool IsVideoBlockType(uint16_t block_type) {
  return block_type == kVideoIBlock ||
         block_type == kVideoPBlock ||
         block_type == kVideoBBlock;
}

/**
 * @brief IsKnownPictureMode validates the encoded picture mode range.
 * @param picture_mode Encoded picture mode value.
 * @return True when the mode is inside the documented range.
 * @throws None.
 */
inline bool IsKnownPictureMode(uint32_t picture_mode) {
  return picture_mode >= kPictureModeMin && picture_mode <= kPictureModeMax;
}

struct FileHeaderView {
  uint32_t start_code;
  uint32_t magic_number;
};

struct GroupHeaderView {
  uint32_t start_code;
  uint32_t raw_frame_num;
  uint32_t frame_num;
  uint32_t raw_is_audio;
  bool is_audio;
  uint32_t raw_block_number;
  uint32_t block_count;
  uint32_t picture_mode;
  uint32_t frame_rate;
};

struct BlockHeaderView {
  uint16_t block_type;
  uint16_t version;
  uint32_t flags;
  uint32_t block_size;
};

/**
 * @brief ParseFileHeader decodes the fixed-size file header.
 * @param base Pointer to the mapped file bytes.
 * @param size Total available byte count.
 * @param out Receives the parsed view on success.
 * @return True when the header is fully available and valid.
 * @throws None.
 */
inline bool ParseFileHeader(const uint8_t* base,
                            std::size_t size,
                            FileHeaderView* out) {
  if (base == NULL || out == NULL || size < kFileHeaderSize) {
    return false;
  }

  FileHeaderView header;
  header.start_code = ReadLe32(base + 0u);
  header.magic_number = ReadLe32(base + 4u);

  if (header.start_code != kStartCodeHkm4 &&
      header.start_code != kStartCodeHkh4) {
    return false;
  }

  *out = header;
  return true;
}

/**
 * @brief ParseGroupHeader decodes a group header at a given byte offset.
 * @param base Pointer to the mapped file bytes.
 * @param size Total available byte count.
 * @param offset Byte offset where the group header should start.
 * @param out Receives the parsed view on success.
 * @return True when the header passes structural validation.
 * @throws None.
 */
inline bool ParseGroupHeader(const uint8_t* base,
                             std::size_t size,
                             std::size_t offset,
                             GroupHeaderView* out) {
  if (base == NULL || out == NULL || offset > size ||
      size - offset < kGroupHeaderSize) {
    return false;
  }

  const uint8_t* header = base + offset;
  GroupHeaderView view;
  view.start_code = ReadLe32(header + 0u);
  view.raw_frame_num = ReadLe32(header + 4u);
  view.raw_is_audio = ReadLe32(header + 12u);
  view.raw_block_number = ReadLe32(header + 16u);
  view.picture_mode = ReadLe32(header + 24u);
  view.frame_rate = ReadLe32(header + 28u);

  if (view.start_code != kGroupStartCode) {
    return false;
  }
  if (view.raw_frame_num < kConstNumberBase ||
      view.raw_block_number < kConstNumberBase + 1u) {
    return false;
  }
  if (!IsKnownPictureMode(view.picture_mode)) {
    return false;
  }
  if (view.frame_rate == 0u || view.frame_rate > 240u) {
    return false;
  }

  view.frame_num = view.raw_frame_num - kConstNumberBase;
  view.is_audio = (view.raw_is_audio != kConstNumberBase);
  view.block_count = view.raw_block_number - kConstNumberBase;

  *out = view;
  return true;
}

/**
 * @brief ParseBlockHeader decodes a block header at a given byte offset.
 * @param base Pointer to the mapped file bytes.
 * @param size Total available byte count.
 * @param offset Byte offset where the block header should start.
 * @param out Receives the parsed view on success.
 * @return True when the header is fully available and passes basic checks.
 * @throws None.
 */
inline bool ParseBlockHeader(const uint8_t* base,
                             std::size_t size,
                             std::size_t offset,
                             BlockHeaderView* out) {
  if (base == NULL || out == NULL || offset > size ||
      size - offset < kBlockHeaderSize) {
    return false;
  }

  const uint8_t* header = base + offset;
  BlockHeaderView view;
  view.block_type = ReadLe16(header + 0u);
  view.version = ReadLe16(header + 2u);
  view.flags = ReadLe32(header + 8u);
  view.block_size = ReadLe32(header + 16u);

  if (!IsKnownBlockType(view.block_type)) {
    return false;
  }
  if (size - offset < kBlockHeaderSize + view.block_size) {
    return false;
  }

  *out = view;
  return true;
}

}  // namespace private_mp4

#endif  // PRIVATE_MP4_FORMAT_H_
