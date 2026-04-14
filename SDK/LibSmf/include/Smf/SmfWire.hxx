#pragma once

#include "SmfTypes.hxx"

#include <cstdint>
#include <vector>

namespace Solstice::Smf::Wire {

void AppendU32(std::vector<std::byte>& b, uint32_t v);
void AppendU64(std::vector<std::byte>& b, uint64_t v);
uint32_t ReadU32(const std::byte* p);
uint64_t ReadU64(const std::byte* p);

void WriteAttributeValue(std::vector<std::byte>& out, SmfAttributeType t, const SmfValue& v);
bool ReadAttributeValue(const std::byte*& p, const std::byte* end, SmfAttributeType t, SmfValue& outVal);

} // namespace Solstice::Smf::Wire
