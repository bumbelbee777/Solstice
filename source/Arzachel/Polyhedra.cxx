#include "Polyhedra.hxx"
#include <cmath>
#include <map>
#include <array>

namespace Solstice::Arzachel {

Generator<MeshData> Cube(float Size, const Seed& S) {
    return Generator<MeshData>([Size](const Seed& SeedParam) {
        MeshData Mesh;
        float Half = Size * 0.5f;

        Mesh.BoundsMin = Math::Vec3(-Half, -Half, -Half);
        Mesh.BoundsMax = Math::Vec3(Half, Half, Half);

        // 8 corner positions
        Math::Vec3 P[8] = {
            Math::Vec3(-Half, -Half, -Half), Math::Vec3(Half, -Half, -Half),
            Math::Vec3(Half, Half, -Half),   Math::Vec3(-Half, Half, -Half),
            Math::Vec3(-Half, -Half, Half),  Math::Vec3(Half, -Half, Half),
            Math::Vec3(Half, Half, Half),    Math::Vec3(-Half, Half, Half)
        };

        // 6 face normals
        Math::Vec3 N[6] = {
            Math::Vec3(0, 0, -1), Math::Vec3(0, 0, 1),
            Math::Vec3(0, -1, 0), Math::Vec3(0, 1, 0),
            Math::Vec3(-1, 0, 0), Math::Vec3(1, 0, 0)
        };

        // UVs
        Math::Vec2 UV[4] = {
            Math::Vec2(0, 0), Math::Vec2(1, 0),
            Math::Vec2(1, 1), Math::Vec2(0, 1)
        };

        // Face indices
        uint32_t FaceIndices[6][4] = {
            {0, 1, 2, 3}, {5, 4, 7, 6},
            {4, 5, 1, 0}, {3, 2, 6, 7},
            {4, 0, 3, 7}, {1, 5, 6, 2}
        };

        for (int F = 0; F < 6; ++F) {
            for (int I = 0; I < 4; ++I) {
                Mesh.Positions.push_back(P[FaceIndices[F][I]]);
                Mesh.Normals.push_back(N[F]);
                Mesh.UVs.push_back(UV[I]);
            }

            uint32_t Base = F * 4;
            Mesh.Indices.push_back(Base + 0);
            Mesh.Indices.push_back(Base + 1);
            Mesh.Indices.push_back(Base + 2);
            Mesh.Indices.push_back(Base + 0);
            Mesh.Indices.push_back(Base + 2);
            Mesh.Indices.push_back(Base + 3);
        }

        Mesh.SubMeshes.emplace_back(0, 0, static_cast<uint32_t>(Mesh.Indices.size()));
        return Mesh;
    });
}

Generator<MeshData> Sphere(float Radius, int Segments, const Seed& S) {
    return Generator<MeshData>([Radius, Segments](const Seed& SeedParam) {
        MeshData Mesh;

        Mesh.BoundsMin = Math::Vec3(-Radius, -Radius, -Radius);
        Mesh.BoundsMax = Math::Vec3(Radius, Radius, Radius);

        for (int Y = 0; Y <= Segments; ++Y) {
            for (int X = 0; X <= Segments; ++X) {
                float XSegment = static_cast<float>(X) / static_cast<float>(Segments);
                float YSegment = static_cast<float>(Y) / static_cast<float>(Segments);
                float XPos = std::cos(XSegment * 2.0f * 3.14159f) * std::sin(YSegment * 3.14159f);
                float YPos = std::cos(YSegment * 3.14159f);
                float ZPos = std::sin(XSegment * 2.0f * 3.14159f) * std::sin(YSegment * 3.14159f);

                Math::Vec3 Pos(XPos * Radius, YPos * Radius, ZPos * Radius);
                Math::Vec3 Normal(XPos, YPos, ZPos);
                Math::Vec2 UV(XSegment, YSegment);

                Mesh.Positions.push_back(Pos);
                Mesh.Normals.push_back(Normal);
                Mesh.UVs.push_back(UV);
            }
        }

        for (int Y = 0; Y < Segments; ++Y) {
            for (int X = 0; X < Segments; ++X) {
                uint32_t I0 = (Y + 1) * (Segments + 1) + X;
                uint32_t I1 = Y * (Segments + 1) + X;
                uint32_t I2 = Y * (Segments + 1) + X + 1;
                uint32_t I3 = (Y + 1) * (Segments + 1) + X + 1;

                Mesh.Indices.push_back(I0);
                Mesh.Indices.push_back(I1);
                Mesh.Indices.push_back(I2);
                Mesh.Indices.push_back(I0);
                Mesh.Indices.push_back(I2);
                Mesh.Indices.push_back(I3);
            }
        }

        Mesh.SubMeshes.emplace_back(0, 0, static_cast<uint32_t>(Mesh.Indices.size()));
        return Mesh;
    });
}

Generator<MeshData> Cylinder(float Radius, float Height, int Segments, const Seed& S) {
    return Generator<MeshData>([Radius, Height, Segments](const Seed& SeedParam) {
        MeshData Mesh;
        float HalfH = Height * 0.5f;

        Mesh.BoundsMin = Math::Vec3(-Radius, -HalfH, -Radius);
        Mesh.BoundsMax = Math::Vec3(Radius, HalfH, Radius);

        // Side vertices
        for (int I = 0; I <= Segments; ++I) {
            float Theta = static_cast<float>(I) / Segments * 2.0f * 3.14159f;
            float X = std::cos(Theta);
            float Z = std::sin(Theta);

            Math::Vec3 Normal(X, 0, Z);
            Mesh.Positions.push_back(Math::Vec3(X * Radius, -HalfH, Z * Radius));
            Mesh.Normals.push_back(Normal);
            Mesh.UVs.push_back(Math::Vec2(static_cast<float>(I)/Segments, 0));

            Mesh.Positions.push_back(Math::Vec3(X * Radius, HalfH, Z * Radius));
            Mesh.Normals.push_back(Normal);
            Mesh.UVs.push_back(Math::Vec2(static_cast<float>(I)/Segments, 1));
        }

        // Side triangles
        for (int I = 0; I < Segments; ++I) {
            uint32_t Base = I * 2;
            Mesh.Indices.push_back(Base);
            Mesh.Indices.push_back(Base + 1);
            Mesh.Indices.push_back(Base + 2);
            Mesh.Indices.push_back(Base + 1);
            Mesh.Indices.push_back(Base + 3);
            Mesh.Indices.push_back(Base + 2);
        }

        // Bottom cap
        uint32_t CenterBottom = static_cast<uint32_t>(Mesh.Positions.size());
        Mesh.Positions.push_back(Math::Vec3(0, -HalfH, 0));
        Mesh.Normals.push_back(Math::Vec3(0, -1, 0));
        Mesh.UVs.push_back(Math::Vec2(0.5f, 0.5f));

        for (int I = 0; I <= Segments; ++I) {
            float Theta = static_cast<float>(I) / Segments * 2.0f * 3.14159f;
            Mesh.Positions.push_back(Math::Vec3(std::cos(Theta) * Radius, -HalfH, std::sin(Theta) * Radius));
            Mesh.Normals.push_back(Math::Vec3(0, -1, 0));
            Mesh.UVs.push_back(Math::Vec2(0, 0));
        }

        for (int I = 0; I < Segments; ++I) {
            Mesh.Indices.push_back(CenterBottom);
            Mesh.Indices.push_back(CenterBottom + I + 2);
            Mesh.Indices.push_back(CenterBottom + I + 1);
        }

        // Top cap
        uint32_t CenterTop = static_cast<uint32_t>(Mesh.Positions.size());
        Mesh.Positions.push_back(Math::Vec3(0, HalfH, 0));
        Mesh.Normals.push_back(Math::Vec3(0, 1, 0));
        Mesh.UVs.push_back(Math::Vec2(0.5f, 0.5f));

        for (int I = 0; I <= Segments; ++I) {
            float Theta = static_cast<float>(I) / Segments * 2.0f * 3.14159f;
            Mesh.Positions.push_back(Math::Vec3(std::cos(Theta) * Radius, HalfH, std::sin(Theta) * Radius));
            Mesh.Normals.push_back(Math::Vec3(0, 1, 0));
            Mesh.UVs.push_back(Math::Vec2(0, 0));
        }

        for (int I = 0; I < Segments; ++I) {
            Mesh.Indices.push_back(CenterTop);
            Mesh.Indices.push_back(CenterTop + I + 1);
            Mesh.Indices.push_back(CenterTop + I + 2);
        }

        Mesh.SubMeshes.emplace_back(0, 0, static_cast<uint32_t>(Mesh.Indices.size()));
        return Mesh;
    });
}

Generator<MeshData> Torus(float MajorRadius, float MinorRadius, int Segments, int Rings, const Seed& S) {
    return Generator<MeshData>([MajorRadius, MinorRadius, Segments, Rings](const Seed& SeedParam) {
        MeshData Mesh;
        float MaxRadius = MajorRadius + MinorRadius;

        Mesh.BoundsMin = Math::Vec3(-MaxRadius, -MinorRadius, -MaxRadius);
        Mesh.BoundsMax = Math::Vec3(MaxRadius, MinorRadius, MaxRadius);

        for (int Ring = 0; Ring <= Rings; ++Ring) {
            float RingAngle = static_cast<float>(Ring) / Rings * 2.0f * 3.14159f;
            float RingCos = std::cos(RingAngle);
            float RingSin = std::sin(RingAngle);
            Math::Vec3 RingCenter(RingCos * MajorRadius, 0, RingSin * MajorRadius);

            for (int Seg = 0; Seg <= Segments; ++Seg) {
                float SegAngle = static_cast<float>(Seg) / Segments * 2.0f * 3.14159f;
                float SegCos = std::cos(SegAngle);
                float SegSin = std::sin(SegAngle);

                Math::Vec3 Pos = RingCenter + Math::Vec3(SegCos * MinorRadius * RingCos, SegSin * MinorRadius, SegCos * MinorRadius * RingSin);
                Math::Vec3 Normal = (Pos - RingCenter).Normalized();
                Math::Vec2 UV(static_cast<float>(Ring) / Rings, static_cast<float>(Seg) / Segments);

                Mesh.Positions.push_back(Pos);
                Mesh.Normals.push_back(Normal);
                Mesh.UVs.push_back(UV);
            }
        }

        for (int Ring = 0; Ring < Rings; ++Ring) {
            for (int Seg = 0; Seg < Segments; ++Seg) {
                uint32_t I0 = Ring * (Segments + 1) + Seg;
                uint32_t I1 = Ring * (Segments + 1) + Seg + 1;
                uint32_t I2 = (Ring + 1) * (Segments + 1) + Seg;
                uint32_t I3 = (Ring + 1) * (Segments + 1) + Seg + 1;

                Mesh.Indices.push_back(I0);
                Mesh.Indices.push_back(I2);
                Mesh.Indices.push_back(I1);
                Mesh.Indices.push_back(I1);
                Mesh.Indices.push_back(I2);
                Mesh.Indices.push_back(I3);
            }
        }

        Mesh.SubMeshes.emplace_back(0, 0, static_cast<uint32_t>(Mesh.Indices.size()));
        return Mesh;
    });
}

Generator<MeshData> Icosphere(float Radius, int Subdivisions, const Seed& S) {
    return Generator<MeshData>([Radius, Subdivisions](const Seed& SeedParam) {
        MeshData Mesh;

        Mesh.BoundsMin = Math::Vec3(-Radius, -Radius, -Radius);
        Mesh.BoundsMax = Math::Vec3(Radius, Radius, Radius);

        const float T = (1.0f + std::sqrt(5.0f)) / 2.0f;

        std::vector<Math::Vec3> Vertices = {
            Math::Vec3(-1, T, 0).Normalized(),
            Math::Vec3(1, T, 0).Normalized(),
            Math::Vec3(-1, -T, 0).Normalized(),
            Math::Vec3(1, -T, 0).Normalized(),
            Math::Vec3(0, -1, T).Normalized(),
            Math::Vec3(0, 1, T).Normalized(),
            Math::Vec3(0, -1, -T).Normalized(),
            Math::Vec3(0, 1, -T).Normalized(),
            Math::Vec3(T, 0, -1).Normalized(),
            Math::Vec3(T, 0, 1).Normalized(),
            Math::Vec3(-T, 0, -1).Normalized(),
            Math::Vec3(-T, 0, 1).Normalized()
        };

        std::vector<std::array<uint32_t, 3>> Faces = {
            {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
            {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
            {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
            {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}
        };

        for (int Sub = 0; Sub < Subdivisions; ++Sub) {
            std::map<std::pair<uint32_t, uint32_t>, uint32_t> EdgeMap;
            std::vector<std::array<uint32_t, 3>> NewFaces;

            auto GetMidpoint = [&](uint32_t V0, uint32_t V1) -> uint32_t {
                if (V0 > V1) std::swap(V0, V1);
                auto Key = std::make_pair(V0, V1);
                if (EdgeMap.find(Key) != EdgeMap.end()) {
                    return EdgeMap[Key];
                }

                Math::Vec3 Mid = (Vertices[V0] + Vertices[V1]).Normalized();
                uint32_t Idx = static_cast<uint32_t>(Vertices.size());
                Vertices.push_back(Mid);
                EdgeMap[Key] = Idx;
                return Idx;
            };

            for (const auto& Face : Faces) {
                uint32_t V0 = Face[0];
                uint32_t V1 = Face[1];
                uint32_t V2 = Face[2];

                uint32_t A = GetMidpoint(V0, V1);
                uint32_t B = GetMidpoint(V1, V2);
                uint32_t C = GetMidpoint(V2, V0);

                NewFaces.push_back({V0, A, C});
                NewFaces.push_back({V1, B, A});
                NewFaces.push_back({V2, C, B});
                NewFaces.push_back({A, B, C});
            }

            Faces = NewFaces;
        }

        for (const auto& V : Vertices) {
            Math::Vec3 Pos = V * Radius;
            Mesh.Positions.push_back(Pos);
            Mesh.Normals.push_back(V);
            Mesh.UVs.push_back(Math::Vec2(0, 0));
        }

        for (const auto& Face : Faces) {
            Mesh.Indices.push_back(Face[0]);
            Mesh.Indices.push_back(Face[1]);
            Mesh.Indices.push_back(Face[2]);
        }

        Mesh.SubMeshes.emplace_back(0, 0, static_cast<uint32_t>(Mesh.Indices.size()));
        return Mesh;
    });
}

Generator<MeshData> Plane(float Width, float Height, const Seed& S) {
    return Generator<MeshData>([Width, Height](const Seed& SeedParam) {
        MeshData Mesh;
        float HalfW = Width * 0.5f;
        float HalfH = Height * 0.5f;

        Mesh.BoundsMin = Math::Vec3(-HalfW, 0, -HalfH);
        Mesh.BoundsMax = Math::Vec3(HalfW, 0, HalfH);

        Math::Vec3 Normal(0, 1, 0);

        Mesh.Positions.push_back(Math::Vec3(-HalfW, 0, -HalfH));
        Mesh.Normals.push_back(Normal);
        Mesh.UVs.push_back(Math::Vec2(0, 0));

        Mesh.Positions.push_back(Math::Vec3(HalfW, 0, -HalfH));
        Mesh.Normals.push_back(Normal);
        Mesh.UVs.push_back(Math::Vec2(Width, 0));

        Mesh.Positions.push_back(Math::Vec3(HalfW, 0, HalfH));
        Mesh.Normals.push_back(Normal);
        Mesh.UVs.push_back(Math::Vec2(Width, Height));

        Mesh.Positions.push_back(Math::Vec3(-HalfW, 0, HalfH));
        Mesh.Normals.push_back(Normal);
        Mesh.UVs.push_back(Math::Vec2(0, Height));

        Mesh.Indices.push_back(0);
        Mesh.Indices.push_back(1);
        Mesh.Indices.push_back(2);
        Mesh.Indices.push_back(0);
        Mesh.Indices.push_back(2);
        Mesh.Indices.push_back(3);

        Mesh.SubMeshes.emplace_back(0, 0, static_cast<uint32_t>(Mesh.Indices.size()));
        return Mesh;
    });
}

} // namespace Solstice::Arzachel
