#pragma once

#include <Smf/SmfMap.hxx>

#include <optional>
#include <string>

namespace Jackhammer::EntityHelpers {

const Solstice::Smf::SmfVec3* TryGetEntityOriginVec3(const Solstice::Smf::SmfEntity& ent);
bool SetEntityOriginVec3(Solstice::Smf::SmfEntity& ent, const Solstice::Smf::SmfVec3& pos);
bool ComputeEntityOriginAabb(const Solstice::Smf::SmfMap& map, Solstice::Smf::SmfVec3& outMin, Solstice::Smf::SmfVec3& outMax);

std::string MakeUniqueEntityName(const Solstice::Smf::SmfMap& map, const std::string& preferred);
std::string MakeUniqueAcousticZoneName(const Solstice::Smf::SmfMap& map, const std::string& preferred);
std::string MakeUniqueAuthoringLightName(const Solstice::Smf::SmfMap& map, const std::string& preferred);
std::string MakeUniqueFluidVolumeName(const Solstice::Smf::SmfMap& map, const std::string& preferred);
std::string MakeUniqueSoftBodyVolumeName(const Solstice::Smf::SmfMap& map, const std::string& preferred);
std::string MakeUniqueVehicleVolumeName(const Solstice::Smf::SmfMap& map, const std::string& preferred);

const char* TryGetEntityDiffuseTexturePath(const Solstice::Smf::SmfEntity& ent);
void SetEntityDiffuseTexturePath(Solstice::Smf::SmfEntity& ent, const std::string& pathUtf8);
const char* TryGetEntityMaterialPath(const Solstice::Smf::SmfEntity& ent);
void SetEntityMaterialPath(Solstice::Smf::SmfEntity& ent, const std::string& pathUtf8);
const char* TryGetEntityNormalTexturePath(const Solstice::Smf::SmfEntity& ent);
void SetEntityNormalTexturePath(Solstice::Smf::SmfEntity& ent, const std::string& pathUtf8);
const char* TryGetEntityRoughnessTexturePath(const Solstice::Smf::SmfEntity& ent);
void SetEntityRoughnessTexturePath(Solstice::Smf::SmfEntity& ent, const std::string& pathUtf8);
const char* TryGetEntityModelAssetPath(const Solstice::Smf::SmfEntity& ent);
void SetEntityModelAssetPath(Solstice::Smf::SmfEntity& ent, const std::string& pathUtf8);

std::string ToMapRelativePathIfPossible(const std::string& assetPathUtf8, const std::optional<std::string>& currentMapPath);
std::string ResolveMapAssetPath(const std::optional<std::string>& currentMapPath, const std::string& pathUtf8);

void SetEntityVec3ByKey(Solstice::Smf::SmfEntity& ent, const char* key, const Solstice::Smf::SmfVec3& v);
void SetEntityFloatByKey(Solstice::Smf::SmfEntity& ent, const char* key, float f);
const Solstice::Smf::SmfVec3* TryGetEntityVec3ByKey(const Solstice::Smf::SmfEntity& ent, const char* key);
float TryGetEntityFloatByKey(const Solstice::Smf::SmfEntity& ent, const char* key, float defaultVal);
float ReadEntityYawDegrees(const Solstice::Smf::SmfEntity& ent);
float ReadEntityPitchDegrees(const Solstice::Smf::SmfEntity& ent);
float ReadEntityRollDegrees(const Solstice::Smf::SmfEntity& ent);
float ReadEntityUniformScale(const Solstice::Smf::SmfEntity& ent);
Solstice::Smf::SmfVec3 ReadEntityScaleVec3(const Solstice::Smf::SmfEntity& ent);
void SetEntityYawDegrees(Solstice::Smf::SmfEntity& ent, float yawDeg);
void SetEntityPitchDegrees(Solstice::Smf::SmfEntity& ent, float pitchDeg);
void SetEntityRollDegrees(Solstice::Smf::SmfEntity& ent, float rollDeg);
void SetEntityUniformScale(Solstice::Smf::SmfEntity& ent, float s);
void SetEntityScaleVec3(Solstice::Smf::SmfEntity& ent, const Solstice::Smf::SmfVec3& v);

} // namespace Jackhammer::EntityHelpers
