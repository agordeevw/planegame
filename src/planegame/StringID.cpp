#include <planegame/StringID.h>

#include <utility>

uint64_t StringIDHasher::operator()(StringID sid) const { return std::hash<uint64_t>()(sid.value); }
