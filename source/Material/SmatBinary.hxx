#pragma once

#include "SmatTypes.hxx"
#include "Material.hxx"

#include <string>

namespace Solstice::Core {

bool WriteSmat(const std::string& FilePath, const Material& Mat, SmatError* OutError = nullptr);

bool ReadSmat(const std::string& FilePath, Material& OutMaterial, SmatError* OutError = nullptr);

} // namespace Solstice::Core
