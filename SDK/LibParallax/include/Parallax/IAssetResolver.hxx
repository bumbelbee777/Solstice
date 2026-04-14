#pragma once

#include "ParallaxTypes.hxx"
#include <string_view>

namespace Solstice::Parallax {

class IAssetResolver {
public:
    virtual ~IAssetResolver() = default;
    virtual bool Resolve(uint64_t assetHash, AssetData& outData) = 0;
    virtual uint64_t HashFromPath(std::string_view path) = 0;
    virtual bool IsLoaded(uint64_t assetHash) const = 0;
};

} // namespace Solstice::Parallax
