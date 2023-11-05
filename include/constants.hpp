#include <cstdint>
#pragma once

static const uint64_t decimals = 8;
static const uint64_t dec_power = 100000000LL;

inline std::string amount_to_string(const uint64_t amount) {
  std::string s = std::to_string(amount);
  if (s.size() < 9)
  {
    s = std::string(9 - s.size(), '0') + s;
  }
  s.insert(s.size() - 8, ".");
  return s;
}
