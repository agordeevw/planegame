#pragma once

#include <cstdint>

struct StringID {
  uint64_t value;

  friend bool operator==(StringID a, StringID b) { return a.value == b.value; }
};

struct StringIDHasher {
  uint64_t operator()(StringID sid) const;
};

static constexpr StringID makeSID(const char str[]) {
  uint64_t ret = 1242ULL;
  for (const char* s = str; *s != 0; s++) {
    ret = uint64_t(ret) ^ (uint64_t(*s) * 12421512ULL) + 12643ULL;
  }
  return StringID{ ret };
}

#define SID(name) \
  StringID { std::integral_constant<uint64_t, makeSID(name).value>::value }
