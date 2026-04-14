#include "WavefrontParser.hxx"
#include <fstream>
#include <map>
#include <Core/Debug/Debug.hxx>

namespace Solstice::Core {

struct VertexKey {
    int PosIdx = 0;
    int UVIdx = 0;
    int NormIdx = 0;

    bool operator<(const VertexKey& Other) const {
        if (PosIdx != Other.PosIdx) return PosIdx < Other.PosIdx;
        if (UVIdx != Other.UVIdx) return UVIdx < Other.UVIdx;
        return NormIdx < Other.NormIdx;
    }
};

static void ParseMTL(const std::filesystem::path& Path, std::vector<ObjMaterial>& OutMaterials) {
    std::ifstream File(Path);
    if (!File.is_open()) {
        SOLSTICE_LOG("WavefrontParser: Could not open MTL file: " + Path.string());
        return;
    }

    ObjMaterial CurrentMat;
    bool MatStarted = false;

    std::string Line;
    while (std::getline(File, Line)) {
        if (Line.empty()) continue;
        std::stringstream SS(Line);
        std::string Token;
        SS >> Token;

        if (Token == "newmtl") {
            if (MatStarted) {
                OutMaterials.push_back(CurrentMat);
            }
            MatStarted = true;
            CurrentMat = ObjMaterial();
            SS >> CurrentMat.Name;
        } else if (Token == "Ka") {
            SS >> CurrentMat.Ambient.x >> CurrentMat.Ambient.y >> CurrentMat.Ambient.z;
        } else if (Token == "Kd") {
            SS >> CurrentMat.Diffuse.x >> CurrentMat.Diffuse.y >> CurrentMat.Diffuse.z;
        } else if (Token == "Ks") {
            SS >> CurrentMat.Specular.x >> CurrentMat.Specular.y >> CurrentMat.Specular.z;
        } else if (Token == "Ns") {
            SS >> CurrentMat.Shininess;
        } else if (Token == "d" || Token == "Tr") {
            SS >> CurrentMat.Alpha;
        } else if (Token == "map_Kd") {
            SS >> CurrentMat.DiffuseMap;
        } else if (Token == "map_Kn" || Token == "norm") {
            SS >> CurrentMat.NormalMap;
        }
    }
    if (MatStarted) {
        OutMaterials.push_back(CurrentMat);
    }
}

WavefrontData WavefrontParser::Parse(const std::filesystem::path& Path) {
    WavefrontData Result;
    std::ifstream File(Path);
    if (!File.is_open()) {
        Result.Error = "Could not open file: " + Path.string();
        SOLSTICE_LOG("WavefrontParser: " + Result.Error);
        return Result;
    }

    std::vector<Math::Vec3> RawPositions;
    std::vector<Math::Vec3> RawNormals;
    std::vector<Math::Vec2> RawUVs;

    std::map<VertexKey, uint32_t> UniqueVertices;
    
    std::string Line;
    while (std::getline(File, Line)) {
        if (Line.empty() || Line[0] == '#') continue;
        std::stringstream SS(Line);
        std::string Token;
        SS >> Token;

        if (Token == "v") {
            Math::Vec3 Pos;
            SS >> Pos.x >> Pos.y >> Pos.z;
            RawPositions.push_back(Pos);
        } else if (Token == "vn") {
            Math::Vec3 Norm;
            SS >> Norm.x >> Norm.y >> Norm.z;
            RawNormals.push_back(Norm);
        } else if (Token == "vt") {
            Math::Vec2 UV;
            SS >> UV.x >> UV.y;
            RawUVs.push_back(UV);
        } else if (Token == "mtllib") {
            std::string MtlFile;
            SS >> MtlFile;
            std::filesystem::path MtlPath = Path.parent_path() / MtlFile;
            ParseMTL(MtlPath, Result.Materials);
        } else if (Token == "usemtl") {
            // Finish current submesh
            if (!Result.SubMeshes.empty()) {
                auto& Last = Result.SubMeshes.back();
                Last.IndexCount = static_cast<uint32_t>(Result.Indices.size()) - Last.IndexStart;
                if (Last.IndexCount == 0) {
                    Result.SubMeshes.pop_back(); // Remove empty submesh
                }
            }
            
            std::string MatName;
            SS >> MatName;
            
            ObjSubMesh SubMesh;
            SubMesh.MaterialName = MatName;
            SubMesh.IndexStart = static_cast<uint32_t>(Result.Indices.size());
            Result.SubMeshes.push_back(SubMesh);
        } else if (Token == "f") {
            // Ensure we have a submesh
            if (Result.SubMeshes.empty()) {
                ObjSubMesh SubMesh;
                SubMesh.MaterialName = "Default";
                SubMesh.IndexStart = 0;
                Result.SubMeshes.push_back(SubMesh);
            }

            std::vector<VertexKey> FaceVertices;
            std::string VertexStr;
            while (SS >> VertexStr) {
                VertexKey Key;
                
                // Split by '/'
                std::vector<std::string> Segments;
                size_t Start = 0;
                size_t End = VertexStr.find('/');
                while (End != std::string::npos) {
                    Segments.push_back(VertexStr.substr(Start, End - Start));
                    Start = End + 1;
                    End = VertexStr.find('/', Start);
                }
                Segments.push_back(VertexStr.substr(Start));

                // Parse indices (1-based to 0-based, handle negatives)
                auto ParseIdx = [&](const std::string& S, size_t Count) -> int {
                    if (S.empty()) return 0;
                    try {
                        int Idx = std::stoi(S);
                        if (Idx < 0) return static_cast<int>(Count) + Idx;
                        return Idx - 1;
                    } catch (...) { return 0; }
                };

                if (Segments.size() > 0) Key.PosIdx = ParseIdx(Segments[0], RawPositions.size());
                if (Segments.size() > 1) Key.UVIdx = ParseIdx(Segments[1], RawUVs.size());
                if (Segments.size() > 2) Key.NormIdx = ParseIdx(Segments[2], RawNormals.size());
                
                FaceVertices.push_back(Key);
            }

            // Triangulate (Fan)
            if (FaceVertices.size() >= 3) {
                for (size_t i = 1; i < FaceVertices.size() - 1; ++i) {
                    VertexKey Keys[3] = { FaceVertices[0], FaceVertices[i], FaceVertices[i+1] };
                    
                    for (int k = 0; k < 3; ++k) {
                        if (UniqueVertices.find(Keys[k]) == UniqueVertices.end()) {
                            uint32_t NewIdx = static_cast<uint32_t>(Result.Positions.size());
                            UniqueVertices[Keys[k]] = NewIdx;
                            
                            // Add vertex data
                            if (Keys[k].PosIdx >= 0 && Keys[k].PosIdx < static_cast<int>(RawPositions.size()))
                                Result.Positions.push_back(RawPositions[Keys[k].PosIdx]);
                            else Result.Positions.push_back(Math::Vec3());

                            if (Keys[k].NormIdx >= 0 && Keys[k].NormIdx < static_cast<int>(RawNormals.size()))
                                Result.Normals.push_back(RawNormals[Keys[k].NormIdx]);
                            else Result.Normals.push_back(Math::Vec3(0, 1, 0));

                            if (Keys[k].UVIdx >= 0 && Keys[k].UVIdx < static_cast<int>(RawUVs.size()))
                                Result.UVs.push_back(RawUVs[Keys[k].UVIdx]);
                            else Result.UVs.push_back(Math::Vec2());
                        }
                        Result.Indices.push_back(UniqueVertices[Keys[k]]);
                    }
                }
            }
        }
    }
    
    // Close last submesh
    if (!Result.SubMeshes.empty()) {
        auto& Last = Result.SubMeshes.back();
        Last.IndexCount = static_cast<uint32_t>(Result.Indices.size()) - Last.IndexStart;
         if (Last.IndexCount == 0) {
            Result.SubMeshes.pop_back();
        }
    }

    Result.Success = true;
    SOLSTICE_LOG("WavefrontParser: Loaded " + Path.string() + " (" + 
                 std::to_string(Result.Positions.size()) + " vertices, " + 
                 std::to_string(Result.Indices.size() / 3) + " triangles)");
    return Result;
}

} // namespace Solstice::Core
