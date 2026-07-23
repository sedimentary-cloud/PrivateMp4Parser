#ifndef PRIVATE_MP4_FRAME_QUERY_H_
#define PRIVATE_MP4_FRAME_QUERY_H_

#include <cstdint>
#include <string>
#include <vector>

namespace private_mp4 {

/**
 * @brief ParseFrameQuery expands comma/range syntax into explicit frame numbers.
 * @param query User-provided expression such as "1,5:8,42".
 * @param frames Receives the expanded frame numbers on success.
 * @param error Receives a human-readable error message on failure.
 * @return True when parsing succeeds.
 * @throws None.
 */
bool ParseFrameQuery(const std::string& query,
                     std::vector<uint32_t>* frames,
                     std::string* error);

}  // namespace private_mp4

#endif  // PRIVATE_MP4_FRAME_QUERY_H_
