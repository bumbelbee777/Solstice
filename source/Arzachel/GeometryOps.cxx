#include "GeometryOps.hxx"
#include "Constraints.hxx"
#include "TerrainGenerator.hxx"
#include "ProceduralTexture.hxx"
#include <Core/BSP.hxx>
#include <Core/Octree.hxx>
#include <algorithm>
#include <cmath>
#include <map>

// Forward declare to avoid ambiguity
namespace Solstice::Arzachel {
    ConstraintResult SatisfyVisualObjective(
        const VisualObjective& Objective,
        const TerrainConstraint& Terrain,
        const Math::Vec3& InitialPosition,
        const Seed& S
    );
}

namespace Solstice::Arzachel {

// Helper to convert MeshData to BSP triangles
static std::vector<Core::Triangle> MeshToBSPTriangles(const MeshData& Mesh) {
    std::vector<Core::Triangle> Triangles;
    for (uint32_t I = 0; I < Mesh.Indices.size(); I += 3) {
        Triangles.emplace_back(
            Mesh.Positions[Mesh.Indices[I]],
            Mesh.Positions[Mesh.Indices[I + 1]],
            Mesh.Positions[Mesh.Indices[I + 2]],
            I / 3
        );
    }
    return Triangles;
}

Generator<MeshData> Merge(const Generator<MeshData>& A, const Generator<MeshData>& B) {
    return Generator<MeshData>([A, B](const Seed& S) {
        Seed SeedA = S.Derive(0);
        Seed SeedB = S.Derive(1);

        MeshData MeshA = A(SeedA);
        MeshData MeshB = B(SeedB);

        MeshData Result;
        uint32_t OffsetB = static_cast<uint32_t>(MeshA.Positions.size());

        Result.Positions = MeshA.Positions;
        Result.Positions.insert(Result.Positions.end(), MeshB.Positions.begin(), MeshB.Positions.end());

        Result.Normals = MeshA.Normals;
        Result.Normals.insert(Result.Normals.end(), MeshB.Normals.begin(), MeshB.Normals.end());

        Result.UVs = MeshA.UVs;
        Result.UVs.insert(Result.UVs.end(), MeshB.UVs.begin(), MeshB.UVs.end());

        Result.Indices = MeshA.Indices;
        for (uint32_t Index : MeshB.Indices) {
            Result.Indices.push_back(Index + OffsetB);
        }

        Result.SubMeshes = MeshA.SubMeshes;
        uint32_t IndexOffset = static_cast<uint32_t>(MeshA.Indices.size());
        for (const auto& Sub : MeshB.SubMeshes) {
            Result.SubMeshes.emplace_back(Sub.MaterialID, Sub.IndexStart + IndexOffset, Sub.IndexCount);
        }

        Result.CalculateBounds();
        return Result;
    });
}

Generator<MeshData> Transform(const Generator<MeshData>& Mesh, const Math::Matrix4& TransformMatrix) {
    return Mesh.Map(std::function<MeshData(MeshData)>([TransformMatrix](MeshData MeshDataParam) {
        MeshData Result = MeshDataParam;
        for (auto& Position : Result.Positions) Position = TransformMatrix.TransformPoint(Position);
        Math::Matrix4 NormalMatrix = TransformMatrix.Inverse().Transposed();
        for (auto& Normal : Result.Normals) Normal = NormalMatrix.TransformVector(Normal).Normalized();
        Result.CalculateBounds();
        return Result;
    }));
}

Generator<MeshData> Instance(const Generator<MeshData>& Mesh, const std::vector<Math::Matrix4>& Transforms) {
    return Generator<MeshData>([Mesh, Transforms](const Seed& SeedParam) {
        MeshData BaseMesh = Mesh(SeedParam.Derive(0));
        MeshData Result;
        uint32_t BaseVertexCount = static_cast<uint32_t>(BaseMesh.Positions.size());
        uint32_t BaseIndexCount = static_cast<uint32_t>(BaseMesh.Indices.size());

        for (size_t I = 0; I < Transforms.size(); ++I) {
            const Math::Matrix4& T = Transforms[I];
            Math::Matrix4 NormalT = T.Inverse().Transposed();
            uint32_t VertexOffset = static_cast<uint32_t>(I * BaseVertexCount);

            for (size_t V = 0; V < BaseMesh.Positions.size(); ++V) {
                Result.Positions.push_back(T.TransformPoint(BaseMesh.Positions[V]));
                Result.Normals.push_back(NormalT.TransformVector(BaseMesh.Normals[V]).Normalized());
                Result.UVs.push_back(BaseMesh.UVs[V]);
            }
            for (uint32_t Index : BaseMesh.Indices) Result.Indices.push_back(Index + VertexOffset);
            uint32_t IndexOffset = static_cast<uint32_t>(I * BaseIndexCount);
            for (const auto& Sub : BaseMesh.SubMeshes) {
                Result.SubMeshes.emplace_back(Sub.MaterialID, Sub.IndexStart + IndexOffset, Sub.IndexCount);
            }
        }
        Result.CalculateBounds();
        return Result;
    });
}

Generator<MeshData> Union(const Generator<MeshData>& A, const Generator<MeshData>& B) {
    // Robust CSG Union using BSP
    return Generator<MeshData>([A, B](const Seed& SeedParam) {
        MeshData MeshA = A(SeedParam.Derive(0));
        MeshData MeshB = B(SeedParam.Derive(1));

        // For now, Merge is a safe fallback until full CSG classification is implemented
        // Proper CSG using BSP:
        // 1. Build BSP for A and B.
        // 2. Clip A against B (keep outside).
        // 3. Clip B against A (keep outside).
        // 4. Merge results.
        return Merge(Generator<MeshData>::Constant(MeshA), Generator<MeshData>::Constant(MeshB))(SeedParam.Derive(2));
    });
}

Generator<MeshData> Difference(const Generator<MeshData>& A, const Generator<MeshData>& B) {
    return A; // Placeholder
}

Generator<MeshData> Intersection(const Generator<MeshData>& A, const Generator<MeshData>& B) {
    return Generator<MeshData>::Constant(MeshData{}); // Placeholder
}

Generator<MeshData> PlaceOnTerrain(const Generator<MeshData>& Mesh, const TerrainConstraint& Terrain) {
    return Mesh.Map(std::function<MeshData(MeshData)>([Terrain](MeshData MeshDataParam) {
        if (MeshDataParam.Positions.empty() || !Terrain.Heightmap || Terrain.Heightmap->empty()) return MeshDataParam;
        MeshData Result = MeshDataParam;
        float MinY = Result.Positions[0].y;
        for (const auto& P : Result.Positions) if (P.y < MinY) MinY = P.y;
        Math::Vec3 Center(0, 0, 0);
        for (const auto& P : Result.Positions) { Center.x += P.x; Center.z += P.z; }
        Center.x /= static_cast<float>(Result.Positions.size());
        Center.z /= static_cast<float>(Result.Positions.size());
        float Height = SampleTerrainHeight(*Terrain.Heightmap, Terrain.Resolution, Terrain.TerrainSize, Center);
        float Offset = Height - MinY;
        for (auto& P : Result.Positions) P.y += Offset;
        Result.CalculateBounds();
        return Result;
    }));
}

Generator<MeshData> WithConstraints(const Generator<MeshData>& Mesh, const std::vector<PositionConstraint>& PCs, const std::vector<DistanceConstraint>& DCs, const TerrainConstraint& TC, const Seed& SP) {
    return Generator<MeshData>([Mesh, PCs, DCs, TC, SP](const Seed& SeedParam) {
        MeshData Base = Mesh(SeedParam.Derive(0));
        if (Base.Positions.empty()) return Base;
        Math::Vec3 Center(0, 0, 0);
        for (const auto& P : Base.Positions) Center += P;
        Center /= static_cast<float>(Base.Positions.size());
        ConstraintResult Res = SatisfyConstraints(PCs, DCs, TC, Center, SP);
        return Transform(Generator<MeshData>::Constant(Base), Math::Matrix4::Translation(Res.FinalPosition - Center))(SeedParam.Derive(1));
    });
}

Generator<MeshData> SatisfyVisualObjective(const Generator<MeshData>& Mesh, const VisualObjective& Obj, const TerrainConstraint& Ter, const Seed& SP) {
    return Generator<MeshData>([Mesh, Obj, Ter, SP](const Seed& SeedParam) {
        MeshData Base = Mesh(SeedParam.Derive(0));
        if (Base.Positions.empty()) return Base;
        Math::Vec3 Center(0, 0, 0);
        for (const auto& P : Base.Positions) Center += P;
        Center /= static_cast<float>(Base.Positions.size());
        ConstraintResult Res = ::Solstice::Arzachel::SatisfyVisualObjective(Obj, Ter, Center, SP);
        return Transform(Generator<MeshData>::Constant(Base), Math::Matrix4::Translation(Res.FinalPosition - Center))(SeedParam.Derive(1));
    });
}

Generator<MeshData> Extrude(const Generator<MeshData>& Mesh, float Distance) {
    return Generator<MeshData>([Mesh, Distance](const Seed& SeedParam) {
        MeshData Input = Mesh(SeedParam);
        if (Input.Positions.empty()) return Input;
        MeshData Result = Input;
        uint32_t Offset = static_cast<uint32_t>(Input.Positions.size());
        for (size_t I = 0; I < Input.Positions.size(); ++I) {
            Result.Positions.push_back(Input.Positions[I] + Input.Normals[I] * Distance);
            Result.Normals.push_back(Input.Normals[I]);
            Result.UVs.push_back(Input.UVs[I]);
        }
        auto Adjacency = Input.FindEdges();
        for (auto const& [EdgeObj, Faces] : Adjacency) {
            if (Faces.size() == 1) {
                uint32_t V0 = EdgeObj.V0, V1 = EdgeObj.V1, V2 = V0 + Offset, V3 = V1 + Offset;
                Result.Indices.push_back(V0); Result.Indices.push_back(V1); Result.Indices.push_back(V3);
                Result.Indices.push_back(V0); Result.Indices.push_back(V3); Result.Indices.push_back(V2);
            }
        }
        for (uint32_t I = 0; I < Input.Indices.size(); I += 3) {
            Result.Indices.push_back(Input.Indices[I] + Offset);
            Result.Indices.push_back(Input.Indices[I + 2] + Offset);
            Result.Indices.push_back(Input.Indices[I + 1] + Offset);
        }
        Result.CalculateBounds();
        return Result;
    });
}

Generator<MeshData> Inset(const Generator<MeshData>& Mesh, float Distance) {
    return Generator<MeshData>([Mesh, Distance](const Seed& SeedParam) {
        MeshData Input = Mesh(SeedParam);
        if (Input.Positions.empty()) return Input;
        MeshData Result;
        for (uint32_t I = 0; I < Input.Indices.size(); I += 3) {
            uint32_t I0 = Input.Indices[I], I1 = Input.Indices[I+1], I2 = Input.Indices[I+2];
            Math::Vec3 V0 = Input.Positions[I0], V1 = Input.Positions[I1], V2 = Input.Positions[I2];
            Math::Vec3 Center = (V0 + V1 + V2) * (1.0f / 3.0f);
            auto InsetV = [&](const Math::Vec3& V) { return V + (Center - V).Normalized() * Distance; };
            uint32_t Base = static_cast<uint32_t>(Result.Positions.size());
            Result.Positions.push_back(V0); Result.Positions.push_back(V1); Result.Positions.push_back(V2);
            Result.Positions.push_back(InsetV(V0)); Result.Positions.push_back(InsetV(V1)); Result.Positions.push_back(InsetV(V2));
            for (int J = 0; J < 6; ++J) { Result.Normals.push_back(Input.Normals[I0]); Result.UVs.push_back(Input.UVs[I0]); }
            auto AddQuad = [&](uint32_t A, uint32_t B, uint32_t C, uint32_t D) {
                Result.Indices.push_back(Base+A); Result.Indices.push_back(Base+B); Result.Indices.push_back(Base+D);
                Result.Indices.push_back(Base+A); Result.Indices.push_back(Base+D); Result.Indices.push_back(Base+C);
            };
            AddQuad(0, 1, 3, 4); AddQuad(1, 2, 4, 5); AddQuad(2, 0, 5, 3);
            Result.Indices.push_back(Base+3); Result.Indices.push_back(Base+4); Result.Indices.push_back(Base+5);
        }
        Result.CalculateBounds();
        return Result;
    });
}

Generator<MeshData> Bevel(const Generator<MeshData>& Mesh, float Distance) {
    // Face-based Bevel: Inset + Offset along normal
    return Generator<MeshData>([Mesh, Distance](const Seed& SeedParam) {
        MeshData Input = Mesh(SeedParam);
        if (Input.Positions.empty()) return Input;
        MeshData Result;
        for (uint32_t I = 0; I < Input.Indices.size(); I += 3) {
            uint32_t I0 = Input.Indices[I], I1 = Input.Indices[I+1], I2 = Input.Indices[I+2];
            Math::Vec3 V0 = Input.Positions[I0], V1 = Input.Positions[I1], V2 = Input.Positions[I2];
            Math::Vec3 Normal = Input.Normals[I0];
            Math::Vec3 Center = (V0 + V1 + V2) * (1.0f / 3.0f);
            auto InsetV = [&](const Math::Vec3& V) { return V + (Center - V).Normalized() * Distance + Normal * Distance; };
            uint32_t Base = static_cast<uint32_t>(Result.Positions.size());
            Result.Positions.push_back(V0); Result.Positions.push_back(V1); Result.Positions.push_back(V2);
            Result.Positions.push_back(InsetV(V0)); Result.Positions.push_back(InsetV(V1)); Result.Positions.push_back(InsetV(V2));
            for (int J = 0; J < 6; ++J) { Result.Normals.push_back(Normal); Result.UVs.push_back(Input.UVs[I0]); }
            auto AddQuad = [&](uint32_t A, uint32_t B, uint32_t C, uint32_t D) {
                Result.Indices.push_back(Base+A); Result.Indices.push_back(Base+B); Result.Indices.push_back(Base+D);
                Result.Indices.push_back(Base+A); Result.Indices.push_back(Base+D); Result.Indices.push_back(Base+C);
            };
            AddQuad(0, 1, 3, 4); AddQuad(1, 2, 4, 5); AddQuad(2, 0, 5, 3);
            Result.Indices.push_back(Base+3); Result.Indices.push_back(Base+4); Result.Indices.push_back(Base+5);
        }
        Result.CalculateBounds();
        return Result;
    });
}

Generator<MeshData> Smooth(const Generator<MeshData>& Mesh, int Iterations) {
    return Mesh.Map(std::function<MeshData(MeshData)>([Iterations](MeshData M) {
        MeshData Result = M;
        for (int Iter = 0; Iter < Iterations; ++Iter) {
            Core::Octree Tree(Result.BoundsMin, Result.BoundsMax);
            for (size_t I = 0; I < Result.Positions.size(); ++I) Tree.Insert(static_cast<uint32_t>(I), Result.Positions[I], Result.Positions[I]);
            std::vector<Math::Vec3> NewPos = Result.Positions;
            for (size_t I = 0; I < Result.Positions.size(); ++I) {
                std::vector<uint32_t> Neighbors; Tree.QuerySphere(Result.Positions[I], 0.1f, Neighbors);
                if (Neighbors.size() <= 1) continue;
                Math::Vec3 Avg(0, 0, 0); for (uint32_t Nb : Neighbors) Avg += Result.Positions[Nb];
                NewPos[I] = Result.Positions[I] * 0.5f + (Avg / static_cast<float>(Neighbors.size())) * 0.5f;
            }
            Result.Positions = NewPos;
        }
        Result.CalculateBounds(); return Result;
    }));
}

Generator<MeshData> Rotate(const Generator<MeshData>& Mesh, float Angle, const Math::Vec3& Axis) {
    return Transform(Mesh, Math::Matrix4::RotationAxis(Axis, Angle * 3.14159f / 180.0f));
}

Generator<MeshData> Scale(const Generator<MeshData>& Mesh, const Math::Vec3& Factors) {
    return Transform(Mesh, Math::Matrix4::Scale(Factors));
}

Generator<MeshData> Simplify(const Generator<MeshData>& Mesh, float Threshold) {
    return Generator<MeshData>([Mesh, Threshold](const Seed& SeedParam) {
        MeshData Input = Mesh(SeedParam);
        if (Input.Positions.empty() || Threshold <= 0.0f) return Input;
        MeshData Result; std::map<std::tuple<int, int, int>, uint32_t> Grid;
        std::vector<uint32_t> Remap(Input.Positions.size());
        for (size_t I = 0; I < Input.Positions.size(); ++I) {
            Math::Vec3 P = Input.Positions[I]; auto Key = std::make_tuple((int)(P.x/Threshold), (int)(P.y/Threshold), (int)(P.z/Threshold));
            if (Grid.count(Key)) Remap[I] = Grid[Key];
            else {
                Remap[I] = static_cast<uint32_t>(Result.Positions.size()); Grid[Key] = Remap[I];
                Result.Positions.push_back(P); Result.Normals.push_back(Input.Normals[I]); Result.UVs.push_back(Input.UVs[I]);
            }
        }
        for (uint32_t I = 0; I < Input.Indices.size(); I += 3) {
            uint32_t V0 = Remap[Input.Indices[I]], V1 = Remap[Input.Indices[I+1]], V2 = Remap[Input.Indices[I+2]];
            if (V0 != V1 && V1 != V2 && V2 != V0) { Result.Indices.push_back(V0); Result.Indices.push_back(V1); Result.Indices.push_back(V2); }
        }
        Result.CalculateBounds(); return Result;
    });
}

Generator<MeshData> Clone(const Generator<MeshData>& Mesh, const Math::Vec3& Offset) {
    return Merge(Mesh, Transform(Mesh, Math::Matrix4::Translation(Offset)));
}

Generator<MeshData> Subdivide(const Generator<MeshData>& Mesh) {
    return Generator<MeshData>([Mesh](const Seed& SeedParam) {
        MeshData Input = Mesh(SeedParam);
        if (Input.Positions.empty()) return Input;
        MeshData Result;
        std::map<MeshData::Edge, uint32_t> Mids;
        auto GetMid = [&](uint32_t A, uint32_t B) {
            MeshData::Edge E(A, B); if (Mids.count(E)) return Mids[E];
            uint32_t Idx = static_cast<uint32_t>(Result.Positions.size());
            Result.Positions.push_back((Input.Positions[A] + Input.Positions[B]) * 0.5f);
            Result.Normals.push_back((Input.Normals[A] + Input.Normals[B]).Normalized());
            Result.UVs.push_back((Input.UVs[A] + Input.UVs[B]) * 0.5f);
            return Mids[E] = Idx;
        };
        Result.Positions = Input.Positions; Result.Normals = Input.Normals; Result.UVs = Input.UVs;
        for (uint32_t I = 0; I < Input.Indices.size(); I += 3) {
            uint32_t V0 = Input.Indices[I], V1 = Input.Indices[I+1], V2 = Input.Indices[I+2];
            uint32_t M0 = GetMid(V0, V1), M1 = GetMid(V1, V2), M2 = GetMid(V2, V0);
            Result.Indices.push_back(V0); Result.Indices.push_back(M0); Result.Indices.push_back(M2);
            Result.Indices.push_back(M0); Result.Indices.push_back(V1); Result.Indices.push_back(M1);
            Result.Indices.push_back(M2); Result.Indices.push_back(M1); Result.Indices.push_back(V2);
            Result.Indices.push_back(M0); Result.Indices.push_back(M1); Result.Indices.push_back(M2);
        }
        Result.CalculateBounds();
        return Result;
    });
}

Generator<MeshData> Mirror(const Generator<MeshData>& Mesh, const Math::Vec4& Plane) {
    return Generator<MeshData>([Mesh, Plane](const Seed& SeedParam) {
        MeshData Input = Mesh(SeedParam);
        if (Input.Positions.empty()) return Input;
        MeshData Result = Input;
        uint32_t Offset = static_cast<uint32_t>(Input.Positions.size());
        Math::Vec3 N(Plane.x, Plane.y, Plane.z);
        for (size_t I = 0; I < Input.Positions.size(); ++I) {
            Math::Vec3 P = Input.Positions[I];
            Result.Positions.push_back(P - N * (2.0f * (P.Dot(N) + Plane.w)));
            Math::Vec3 Norm = Input.Normals[I];
            Result.Normals.push_back(Norm - N * (2.0f * Norm.Dot(N)));
            Result.UVs.push_back(Input.UVs[I]);
        }
        for (uint32_t I = 0; I < Input.Indices.size(); I += 3) {
            Result.Indices.push_back(Input.Indices[I+2] + Offset);
            Result.Indices.push_back(Input.Indices[I+1] + Offset);
            Result.Indices.push_back(Input.Indices[I] + Offset);
        }
        Result.CalculateBounds();
        return Result;
    });
}

Generator<MeshData> Pipes(const std::vector<Math::Vec3>& Path, float Radius, int Segments) {
    return Generator<MeshData>([Path, Radius, Segments](const Seed& SeedParam) {
        MeshData Result;
        if (Path.size() < 2) return Result;
        for (size_t I = 0; I < Path.size() - 1; ++I) {
            Math::Vec3 Start = Path[I], End = Path[I + 1], Dir = (End - Start).Normalized();
            Math::Vec3 Up = std::abs(Dir.y) < 0.99f ? Math::Vec3(0, 1, 0) : Math::Vec3(1, 0, 0);
            Math::Vec3 Right = Dir.Cross(Up).Normalized(); Up = Right.Cross(Dir).Normalized();
            uint32_t Base = static_cast<uint32_t>(Result.Positions.size());
            for (int J = 0; J <= Segments; ++J) {
                float Angle = (float)J / Segments * 2.0f * 3.14159f;
                Math::Vec3 Normal = Right * std::cos(Angle) + Up * std::sin(Angle);
                Result.Positions.push_back(Start + Normal * Radius); Result.Positions.push_back(End + Normal * Radius);
                Result.Normals.push_back(Normal); Result.Normals.push_back(Normal);
                Result.UVs.push_back(Math::Vec2((float)J/Segments, 0)); Result.UVs.push_back(Math::Vec2((float)J/Segments, 1));
            }
            for (int J = 0; J < Segments; ++J) {
                uint32_t V0 = Base + J * 2, V1 = Base + J * 2 + 1, V2 = Base + (J + 1) * 2, V3 = Base + (J + 1) * 2 + 1;
                Result.Indices.push_back(V0); Result.Indices.push_back(V1); Result.Indices.push_back(V2);
                Result.Indices.push_back(V1); Result.Indices.push_back(V3); Result.Indices.push_back(V2);
            }
        }
        Result.CalculateBounds();
        return Result;
    });
}

Generator<MeshData> Panels(const Generator<MeshData>& Mesh, float Thickness, float InsetDistance) {
    return Generator<MeshData>([Mesh, Thickness, InsetDistance](const Seed& SeedParam) {
        return Inset(Extrude(Mesh, Thickness), InsetDistance)(SeedParam);
    });
}

Generator<MeshData> LSystemTree(const Seed& SeedParam, int Iterations) {
    return Generator<MeshData>([SeedParam, Iterations](const Seed& SeedParamInner) {
        std::vector<Math::Vec3> Path = { Math::Vec3(0, 0, 0), Math::Vec3(0, 1, 0) };
        return Pipes(Path, 0.1f)(SeedParamInner);
    });
}

Generator<MeshData> HeightmapTerrain(const Seed& SeedParam, float Size, float MaxHeight) {
    return Generator<MeshData>([SeedParam, Size, MaxHeight](const Seed& SeedParamInner) {
        MeshData Result; int Res = 32; float Step = Size / Res;
        for (int Y = 0; Y <= Res; ++Y) {
            for (int X = 0; X <= Res; ++X) {
                float NX = (float)X / Res, NY = (float)Y / Res;
                float H = (ProceduralTexture::PerlinNoise2D(NX, NY, 4, 2.0f, SeedParam) + 1.0f) * 0.5f * MaxHeight;
                Result.Positions.push_back(Math::Vec3(X * Step - Size * 0.5f, H, Y * Step - Size * 0.5f));
                Result.Normals.push_back(Math::Vec3(0, 1, 0)); Result.UVs.push_back(Math::Vec2(NX, NY));
            }
        }
        for (int Y = 0; Y < Res; ++Y) {
            for (int X = 0; X < Res; ++X) {
                uint32_t I0 = Y * (Res + 1) + X, I1 = Y * (Res + 1) + X + 1, I2 = (Y + 1) * (Res + 1) + X, I3 = (Y + 1) * (Res + 1) + X + 1;
                Result.Indices.push_back(I0); Result.Indices.push_back(I2); Result.Indices.push_back(I1);
                Result.Indices.push_back(I1); Result.Indices.push_back(I2); Result.Indices.push_back(I3);
            }
        }
        Result.CalculateBounds();
        return Result;
    });
}

Generator<MeshData> GenerateLOD(const Generator<MeshData>& Mesh, int Level) {
    return Generator<MeshData>([Mesh, Level](const Seed& SeedParam) {
        MeshData Base = Mesh(SeedParam);
        if (Level == 0) return Base;
        return Simplify(Generator<MeshData>::Constant(Base), Level * 0.2f)(SeedParam);
    });
}

Generator<MeshData> Inflate(const Generator<MeshData>& Mesh, float Distance) {
    return Mesh.Map(std::function<MeshData(MeshData)>([Distance](MeshData M) {
        MeshData Result = M;
        for (size_t I = 0; I < Result.Positions.size(); ++I) Result.Positions[I] += Result.Normals[I] * Distance;
        Result.CalculateBounds(); return Result;
    }));
}

Generator<MeshData> Pinch(const Generator<MeshData>& Mesh, float Factor, const Math::Vec3& Point) {
    return Mesh.Map(std::function<MeshData(MeshData)>([Factor, Point](MeshData M) {
        MeshData Result = M;
        for (auto& P : Result.Positions) {
            Math::Vec3 Dir = Point - P; float Dist = Dir.Length();
            if (Dist > 0.001f) P += Dir.Normalized() * (Factor / (Dist + 1.0f));
        }
        Result.CalculateBounds(); return Result;
    }));
}

Generator<MeshData> Twist(const Generator<MeshData>& Mesh, float Angle, const Math::Vec3& Axis) {
    return Mesh.Map(std::function<MeshData(MeshData)>([Angle, Axis](MeshData M) {
        MeshData Result = M; float Rad = Angle * 3.14159f / 180.0f;
        for (auto& P : Result.Positions) {
            float H = P.Dot(Axis); Math::Matrix4 Rot = Math::Matrix4::RotationAxis(Axis, H * Rad);
            P = Rot.TransformPoint(P);
        }
        Result.CalculateBounds(); return Result;
    }));
}

} // namespace Solstice::Arzachel
