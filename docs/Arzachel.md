# Arzachel Procedural Generation System

Arzachel is a functional, deterministic procedural generation system for 3D meshes, textures, and materials. It uses a `Generator<T>` model where assets are pure functions of a `Seed`.

## Mesh Generation API

The system provides a fluent, Blender-like API for constructing and editing meshes from code.

### Base Primitives

```cpp
using namespace Solstice::Arzachel;

Seed MySeed(12345);
MeshGenerator MyMesh = Cube(1.0f, MySeed);
// Other primitives: Sphere, Cylinder, Torus, Icosphere, Plane
```

### Modeling Operations

Mesh operations can be chained to create complex geometry:

```cpp
auto Sword = Cube(1.0f, MySeed)
    .Scale({0.1f, 2.0f, 0.05f})  // Blade shape
    .Extrude(0.5f)               // Add point
    .Bevel(0.05f)                // Chamfer edges
    .Smooth(2)                   // Sculpt
    .Build(MySeed);
```

#### Available Operations:
- `Extrude(Distance)`: Pushes faces along their normals.
- `Inset(Distance)`: Creates smaller faces within existing ones.
- `Bevel(Distance)`: Chamfers edges and corners.
- `Smooth(Iterations)`: Applies Laplacian smoothing (Octree-accelerated).
- `Rotate(Angle, Axis)`: Rotates the geometry.
- `Scale(Factors)`: Scales the geometry.
- `Subdivide()`: Increases mesh resolution via midpoint subdivision.
- `Mirror(Plane)`: Mirrors geometry across a plane.
- `Clone(Offset)`: Creates a merged copy with an offset.
- `Inflate(Distance)`: Moves vertices along normals.
- `Twist(Angle, Axis)`: Rotational deformation along an axis.
- `Pinch(Factor, Point)`: Pulls vertices toward a point.

## Complex Asset Builders

Arzachel includes high-level generators for entire game assets:

```cpp
auto MyBuilding = Building(MySeed, 5).Build(MySeed);
auto MyCar = Car(MySeed).Build(MySeed);
auto DamagedCar = Damaged(Car(MySeed), MySeed, 0.2f).Build(MySeed);
```

### Industrial & Scenic Primitives
- `Pipes(Path, Radius)`: Generates connected pipe geometry along a 3D path.
- `LSystemTree(Seed, Iterations)`: Procedural vegetation.
- `HeightmapTerrain(Seed, Size, MaxHeight)`: Large-scale environments.

## Animation & Rigging

Arzachel supports automated rigging and advanced animation blending. Skeleton and posing types (BoneID, Bone, Skeleton, BoneTransform, Pose, BlendPoses, SolveIK) live in the **Skeleton** module (`Solstice::Skeleton`). Arzachel owns animation clips, bone-pattern tracks, and rigging (MergeSkeletons, RemapWeights, SkinWeights, RiggedMesh); Skeleton owns the bone tree and pose types. See [Skeleton.md](Skeleton.md) for the Skeleton API.

```cpp
using namespace Solstice::Arzachel;
using namespace Solstice::Skeleton;

auto Human = HumanoidRig(MySeed).Build(MySeed);
// Includes auto-generated skeleton and proximity-based weights

// Animation blending (Pose and BlendPoses from Solstice::Skeleton)
Pose PoseA = ClipA.Evaluate(Time, Skel);
Pose PoseB = ClipB.Evaluate(Time, Skel);
Pose Mixed = BlendPoses(PoseA, PoseB, 0.5f);
```

## Level of Detail (LOD)

LODs are generated automatically using spatial partitioning:

```cpp
auto HighPoly = ComplexAsset(MySeed);
auto LowPoly = GenerateLOD(HighPoly, 2); // 2nd level of simplification
```

## Procedural Textures & Materials

Textures are generated as raw `TextureData` and can be coordinated into PBR materials targeting a gritty '00s / HL2 Beta aesthetic.

```cpp
Seed MaterialSeed(9876);
// Industrial material with rust and grime
ProceduralMaterial Rust = MaterialGenerator::GenerateIndustrial(MaterialSeed, 1024);

// Scenic material with rock and snow
ProceduralMaterial Mountain = MaterialGenerator::GenerateNatural(MaterialSeed, 1024);

// Add burn/damage to an existing material
ProceduralMaterial Burnt = MaterialGenerator::GenerateDamage(Rust, MaterialSeed, 0.3f);
```

## Spatial Partitioning

Arzachel uses `Core::Octree` internally for:
- Accelerated vertex neighbor searches in `Smooth()`.
- Efficient raycasting for `Simplify()` and CSG operations.
- Frustum/Box queries for large-scale environment generation.

## CSG (Constructive Solid Geometry)

Robust Boolean operations using BSP (Binary Space Partitioning) trees:

```cpp
auto UnionMesh = Union(MeshA, MeshB);
auto DiffMesh = Difference(MeshA, MeshB);
auto IntersectMesh = Intersection(MeshA, MeshB);
```
