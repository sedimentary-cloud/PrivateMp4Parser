#include "frame_query.h"

#include <cctype>
#include <cstdlib>
#include <limits>
#include <sstream>

namespace private_mp4 {

namespace {

/**
 * @brief TrimCopy removes ASCII whitespace from both ends of a string.
 * @param text Source text.
 * @return Trimmed copy.
 * @throws None.
 */
std::string TrimCopy(const std::string& text) {
  std::size_t first = 0u;
  while (first < text.size() &&
         std::isspace(static_cast<unsigned char>(text[first])) != 0) {
    ++first;
  }

  std::size_t last = text.size();
  while (last > first &&
         std::isspace(static_cast<unsigned char>(text[last - 1u])) != 0) {
    --last;
  }

  return text.substr(first, last - first);
}

/**
 * @brief ParseUint32 parses a decimal unsigned integer.
 * @param text Decimal token to parse.
 * @param value Receives the parsed number on success.
 * @return True when parsing succeeds without overflow.
 * @throws None.
 */
bool ParseUint32(const std::string& text, uint32_t* value) {
  if (value == NULL || text.empty()) {
    return false;
  }

  uint64_t parsed = 0u;
  for (std::size_t i = 0; i < text.size(); ++i) {
    const char ch = text[i];
    if (ch < '0' || ch > '9') {
      return false;
    }
    parsed = parsed * 10u + static_cast<uint64_t>(ch - '0');
    if (parsed > std::numeric_limits<uint32_t>::max()) {
      return false;
    }
  }

  *value = static_cast<uint32_t>(parsed);
  return true;
}

/**
 * @brief SplitByComma splits a query into comma-separated tokens.
 * @param query Full user query.
 * @param tokens Receives each token, including empty tokens.
 * @return None.
 * @throws None.
 */
void SplitByComma(const std::string& query, std::vector<std::string>* tokens) {
  std::size_t token_begin = 0u;
  while (token_begin <= query.size()) {
    const std::size_t comma = query.find(',', token_begin);
    if (comma == std::string::npos) {
      tokens->push_back(query.substr(token_begin));
      break;
    }
    tokens->push_back(query.substr(token_begin, comma - token_begin));
    token_begin = comma + 1u;
  }
}

}  // namespace

bool ParseFrameQuery(const std::string& query,
                     std::vector<uint32_t>* frames,
                     std::string* error) {
  if (frames == NULL || error == NULL) {
    return false;
  }

  frames->clear();
  error->clear();

  const std::string trimmed_query = TrimCopy(query);
  if (trimmed_query.empty()) {
    *error = "Frame query is empty.";
    return false;
  }

  std::vector<std::string> tokens;
  SplitByComma(trimmed_query, &tokens);

  for (std::size_t i = 0; i < tokens.size(); ++i) {
    const std::string token = TrimCopy(tokens[i]);
    if (token.empty()) {
      *error = "Frame query contains an empty token.";
      return false;
    }

    const std::size_t colon = token.find(':');
    if (colon == std::string::npos) {
      uint32_t frame = 0u;
      if (!ParseUint32(token, &frame)) {
        *error = "Invalid frame token: " + token;
        return false;
      }
      frames->push_back(frame);
      continue;
    }

    if (token.find(':', colon + 1u) != std::string::npos) {
      *error = "Only one ':' is allowed in a range token: " + token;
      return false;
    }

    const std::string begin_text = TrimCopy(token.substr(0u, colon));
    const std::string end_text = TrimCopy(token.substr(colon + 1u));
    uint32_t begin = 0u;
    uint32_t end = 0u;
    if (!ParseUint32(begin_text, &begin) || !ParseUint32(end_text, &end)) {
      *error = "Invalid range token: " + token;
      return false;
    }
    if (begin > end) {
      *error = "Range begin is greater than range end: " + token;
      return false;
    }

    for (uint32_t frame = begin; frame <= end; ++frame) {
      frames->push_back(frame);
      if (frame == std::numeric_limits<uint32_t>::max()) {
        break;
      }
    }
  }

  return true;
}

}  // namespace private_mp4
