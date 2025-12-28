#pragma once

#include "../Solstice.hxx"
#include "Seed.hxx"
#include "ProceduralTexture.hxx"
#include <bgfx/bgfx.h>
#include <cstdint>

namespace Solstice::Arzachel::MaterialPresets {

// Raw data generation (safe for background threads)
SOLSTICE_API TextureData SnowData(const Seed& SeedVal, uint32_t Resolution = 512);
SOLSTICE_API TextureData IceData(const Seed& SeedVal, uint32_t Resolution = 512);
SOLSTICE_API TextureData RockData(const Seed& SeedVal, uint32_t Resolution = 512);
SOLSTICE_API TextureData DirtData(const Seed& SeedVal, uint32_t Resolution = 512);
SOLSTICE_API TextureData WoodData(const Seed& SeedVal, uint32_t Resolution = 512);
SOLSTICE_API TextureData FrostData(const Seed& SeedVal, uint32_t Resolution = 512);

// Early '00s aesthetic materials
SOLSTICE_API TextureData ConcreteData(const Seed& SeedVal, uint32_t Resolution = 512);
SOLSTICE_API TextureData MetalData(const Seed& SeedVal, uint32_t Resolution = 512);
SOLSTICE_API TextureData PlasticData(const Seed& SeedVal, uint32_t Resolution = 512);
SOLSTICE_API TextureData GlassData(const Seed& SeedVal, uint32_t Resolution = 256);
SOLSTICE_API TextureData RubberData(const Seed& SeedVal, uint32_t Resolution = 256);

// GPU handle generation (must be on main thread)
SOLSTICE_API bgfx::TextureHandle Snow(const Seed& SeedVal, uint32_t Resolution = 512);
SOLSTICE_API bgfx::TextureHandle Ice(const Seed& SeedVal, uint32_t Resolution = 512);
SOLSTICE_API bgfx::TextureHandle Rock(const Seed& SeedVal, uint32_t Resolution = 512);
SOLSTICE_API bgfx::TextureHandle Dirt(const Seed& SeedVal, uint32_t Resolution = 512);
SOLSTICE_API bgfx::TextureHandle Wood(const Seed& SeedVal, uint32_t Resolution = 512);
SOLSTICE_API bgfx::TextureHandle Frost(const Seed& SeedVal, uint32_t Resolution = 512);

// Early '00s aesthetic materials (GPU handles)
SOLSTICE_API bgfx::TextureHandle Concrete(const Seed& SeedVal, uint32_t Resolution = 512);
SOLSTICE_API bgfx::TextureHandle Metal(const Seed& SeedVal, uint32_t Resolution = 512);
SOLSTICE_API bgfx::TextureHandle Plastic(const Seed& SeedVal, uint32_t Resolution = 512);
SOLSTICE_API bgfx::TextureHandle Glass(const Seed& SeedVal, uint32_t Resolution = 256);
SOLSTICE_API bgfx::TextureHandle Rubber(const Seed& SeedVal, uint32_t Resolution = 256);

} // namespace Solstice::Arzachel::MaterialPresets
