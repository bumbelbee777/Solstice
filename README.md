# **Solstice Engine**

*A CPU-centric software physics and rendering engine for 480p–720p retro-modern graphics.*

## **Overview**

**Solstice** is a custom engine designed to achieve **PS2 → early PS3–level visuals** and gameplay entirely on the CPU (with optional iGPU assist). It combines **classic retro constraints** (baked lighting, compact textures, BSPs) with **modern HPC techniques** (SoA/AoSoA memory layouts, SIMD, async batching, bitmask acceleration).

Target platforms: modern x86-64 CPUs (8+ cores) at 480p–720p (1080p tech-demo mode).
Target use cases: stylized 3D games, experimental renderers, or “next-gen retro” projects.

## **Rendering Architecture**

### **1. Scene Management**

* **Spatial Partitioning:** BSP + Octree for coarse pruning.
* **Culling:** SIMD frustum tests + software occlusion (low-res Hi-Z buffer).
* **LOD:** Aggressive multi-tier system (geom, texture, feature toggling).
* **Data Layout:**

  * **SoA/AoSoA** for transforms, AABBs, vertex attributes → SIMD-friendly.
  * **AoS** only for tightly coupled per-triangle/per-pixel data.

### **2. Rasterization Pipeline**

* **Tile-based software rasterizer** (8×8 or 16×16).
* **Binning:** triangles grouped into tile lists for cache locality.
* **Inner loop:**

  * Fixed-point edge functions + depth interpolation.
  * Early-Z rejection.
  * SIMD-accelerated attribute interpolation.
* **G-buffer lite:** Albedo, normal (oct-encoded), material ID, depth.

### **3. Lighting & Shading**

* **Primary:** baked lightmaps (RGBM) + ambient probes.
* **Dynamic lights:** 1–4 small point/spot lights blended per tile.
* **Shadows:**

  * Static → baked.
  * Dynamic → blob/projected or bitmask ray visibility checks.
* **Shading model:** Lambert diffuse + Blinn-Phong specular; optional normal maps.

### **4. Bitplane Raytracing (Bitwise Wizardry)**

* **Voxel/Cluster bitmasks** used for fast visibility and shadow checks.
* **Bitwise ray marching:** rays represented as masks; traversal = shifts & AND ops.
* **Packet rays:** groups of rays tested together with bitmask propagation.
* **Use cases:**

  * Cheap shadow rays.
  * Ambient occlusion approximation.
  * Portal/visibility queries.

### **5. Post Processing**

* **Upscaling:** internal 480p/540p → output 720p (FSR-like edge-aware upscale).
* **Temporal AA (TAA)** + jittered reprojection for stability.
* **Optional effects:** bloom, tonemap, fog.

## **Physics & Collision**

### **1. Broadphase**

* World quantized into voxel/bin grids.
* **Bitmask occupancy** used for broadphase overlaps:

  ```c
  if (a->MaskRow & b->MaskRow) → possible collision.  
  ```
* SIMD batch AABB tests for object culling.

### **2. Midphase**

* BVH / AABB tree per object.
* Only tested on broadphase candidates.

### **3. Narrowphase**

* SIMD GJK for convex collisions.
* SAT or triangle tests only for critical objects.
* Characters/props: simplified convex proxies.

## **Concurrency & Memory Model**

### **1. Job System**

* Lock-free queues with per-core allocators.
* Frame graph (example 8C/16T):

  * Main: input, AI, job kickoff.
  * Workers: culling, binning, raster.
  * Post workers: lighting, upscale, tonemap.
  * Async threads: asset streaming, decompression.

### **2. Zero-Copy Streaming**

* Memory-mapped assets → pointers used directly.
* No per-frame allocations; pre-allocated pools.
* Command buffers: single contiguous block parsed in place.

### **3. Memory Layout**

* **Vertex data:** quantized fixed-point (16–24b pos, 16b UV, oct16 normals).
* **Textures:** ≤512×512, with mipmaps.
* **Materials:** compact (≤32B) per entry.

## **Performance Targets**

* **720p60** for “PS2.5 → early PS3-lite” visuals.
* **1080p30** for stress/tech demos.
* Geometry budget: \~20–50k visible triangles per frame.
* Lighting budget: 1 sun, 4 small dynamic lights, baked lightmaps.
* Physics budget: \~1–2ms per frame for gameplay collisions.

## **Design Philosophy**

* **Retro spirit, modern efficiency.** Limit resolution, texture sizes, and dynamic lights — then claw back performance with HPC-style memory layouts, SIMD, and batching.
* **Hybrid raster + bitplane rays.** Use rasterization for pixels, bitwise tricks for shadows/visibility/AO.
* **No wasted cycles.** Zero-copy, cache-friendly layouts, async batching, and aggressive culling keep CPU cores busy without thrashing memory.

## **Feature Summary**

✅ Tile-based software rasterizer (SIMD, fixed-point inner loops)
✅ Baked lightmaps + probes, with limited dynamic lights
✅ Bitplane raytracing for shadows, AO, and visibility checks
✅ SoA/AoSoA data layouts for cache-friendly SIMD
✅ BSP + frustum + occlusion culling + aggressive LOD
✅ Zero-copy async streaming and job system
✅ Physics with bitmask broadphase + SIMD narrowphase

**Solstice** is not a GPU replacement — it’s a deliberate throwback reimagined with modern CPU horsepower. By carefully constraining scope and exploiting CPU parallelism, it achieves a sweet spot: visuals far beyond PS2, comparable to early PS3 launch titles, while staying lightweight and scalable on today’s processors.

# AssetLoader - glTF 2.0 Asset Loading System

The AssetLoader provides a complete implementation for loading glTF 2.0 files (.gltf and .glb) into the Solstice engine using the tinygltf library.

## Features

### Supported
- ✅ **Mesh Data**: Positions, normals, UVs (TEXCOORD_0)
- ✅ **Index Buffers**: All component types (uint8, uint16, uint32)
- ✅ **Multiple Primitives**: Submeshes with different materials
- ✅ **PBR Materials**: Metallic/roughness workflow
- ✅ **Material Properties**: Base color, roughness, metallic, emission
- ✅ **Texture References**: Texture indices for materials
- ✅ **Scene Hierarchy**: Node structure with transforms
- ✅ **Both Formats**: ASCII (.gltf) and binary (.glb)
- ✅ **Error Handling**: Detailed error messages

### Not Yet Supported
- ❌ Animations
- ❌ Skins/Skeletal animation
- ❌ Morph targets
- ❌ Multiple UV sets (only TEXCOORD_0)
- ❌ Vertex colors
- ❌ Tangent data
- ❌ Cameras and lights from glTF

## Basic Usage

### 1. Set Asset Path (Optional)

```cpp
#include <Render/Assetloader.hxx>

// Set the base path for all asset loading
AssetLoader::SetAssetPath("./assets/models");

// Get the current asset path
auto path = AssetLoader::GetAssetPath();
```

### 2. Load a Single Mesh

```cpp
// Load just the first mesh from a glTF file
auto MeshPtr = AssetLoader::LoadMesh("sword.gltf");

if (MeshPtr) {
    std::cout << "Vertices: " << MeshPtr->GetVertexCount() << std::endl;
    std::cout << "Triangles: " << MeshPtr->GetTriangleCount() << std::endl;
}
```

### 3. Load Complete Scene

```cpp
// Load the entire glTF file with all data
auto Result = AssetLoader::LoadGLTF("character.glb");

if (Result.Success) {
    // Access meshes
    for (auto& mesh : Result.Meshes) {
        // Use the mesh...
    }
    
    // Access materials
    for (auto& material : Result.Materials) {
        auto color = material.GetAlbedoColor();
        // Use the material...
    }
    
    // Access scene hierarchy
    for (int rootIdx : Result.RootNodes) {
        auto& node = Result.Nodes[rootIdx];
        // Process node hierarchy...
    }
} else {
    std::cerr << "Error: " << Result.ErrorMessage << std::endl;
}
```

### 4. Load Into Libraries

```cpp
MeshLibrary meshLib;
MaterialLibrary matLib;

// Load directly into existing libraries
int meshCount = AssetLoader::LoadGLTFIntoLibraries(
    "environment.gltf",
    meshLib,
    matLib
);

if (meshCount > 0) {
    std::cout << "Loaded " << meshCount << " meshes" << std::endl;
}
```

## API Reference

### Static Methods

#### `void SetAssetPath(const std::filesystem::path& Path)`
Sets the base directory for asset loading. Relative paths in Load* functions will be resolved against this path.

**Default**: `"./assets"`

#### `const std::filesystem::path& GetAssetPath()`
Returns the current asset base path.

#### `AssetLoadResult LoadGLTF(const std::filesystem::path& Path)`
Loads a complete glTF file and returns all contained data.

**Parameters**:
- `Path`: Path to .gltf or .glb file (can be relative to asset path or absolute)

**Returns**: `AssetLoadResult` containing:
- `Meshes`: Vector of loaded mesh objects
- `Materials`: Vector of loaded materials
- `TexturePaths`: Vector of texture file paths
- `Nodes`: Scene hierarchy nodes
- `RootNodes`: Indices of root nodes
- `Success`: Whether loading succeeded
- `ErrorMessage`: Error description if failed

#### `std::unique_ptr<Mesh> LoadMesh(const std::filesystem::path& Path)`
Convenience function to load just the first mesh from a glTF file.

**Returns**: Mesh pointer or `nullptr` on failure

#### `std::vector<Material> LoadMaterials(const std::filesystem::path& Path)`
Loads only the materials from a glTF file.

**Returns**: Vector of materials (empty on failure)

#### `int LoadGLTFIntoLibraries(const std::filesystem::path& Path, MeshLibrary& MeshLib, MaterialLibrary& MatLib)`
Loads a glTF file directly into existing mesh and material libraries.

**Returns**: Number of meshes loaded, or -1 on error

#### `void ClearCache()`
Clears the internal asset cache (future use for resource management).

## Data Structures

### AssetLoadResult

```cpp
struct AssetLoadResult {
    std::vector<std::unique_ptr<Mesh>> Meshes;
    std::vector<Material> Materials;
    std::vector<std::string> TexturePaths;
    
    struct Node {
        std::string Name;
        int MeshIndex = -1;           // Index into Meshes array
        int MaterialIndex = -1;        // Index into Materials array
        float Transform[16];           // 4x4 matrix (column-major)
        std::vector<int> Children;     // Child node indices
    };
    std::vector<Node> Nodes;
    std::vector<int> RootNodes;       // Indices into Nodes array
    
    bool Success = false;
    std::string ErrorMessage;
};
```

## Implementation Details

### Vertex Data Conversion
- glTF vertex positions → `Math::Vec3`
- glTF normals → `Math::Vec3` (defaults to up vector if missing)
- glTF UVs (TEXCOORD_0) → `Math::Vec2` (defaults to (0,0) if missing)
- All data converted to our `QuantizedVertex` format (32-bit floats)

### Material Conversion
glTF PBR materials are converted to our compact `Material` format:
- Base color factor → Albedo color
- Roughness factor → Roughness
- Metallic factor → Metallic factor
- Emissive factor → Emission color/strength
- Base color texture → Albedo texture index
- Normal texture → Normal map index
- Double-sided flag → Material flags
- Alpha mode (blend/mask) → Material flags

### Index Buffer Handling
Supports all glTF index component types:
- `UNSIGNED_BYTE` (uint8)
- `UNSIGNED_SHORT` (uint16)
- `UNSIGNED_INT` (uint32)

All indices are converted to our internal `uint32_t` format.

### Mesh Primitives
Each glTF primitive becomes a submesh in our mesh format:
- Multiple primitives per mesh are supported
- Each primitive can have its own material
- Indices are properly offset for multiple primitives

### Scene Hierarchy
The node hierarchy is preserved with:
- Node names
- Mesh assignments
- Transform matrices (4x4 column-major)
- Parent-child relationships

## Error Handling

The loader provides detailed error messages for:
- File not found
- Unsupported file format
- glTF parsing errors from tinygltf
- Missing required data (e.g., no positions)

Always check `Result.Success` before using loaded data.

## Examples

See `AssetLoaderExamples.cxx` for comprehensive usage examples including:
1. Loading a single mesh
2. Loading a complete scene
3. Loading into libraries
4. Adding to existing scenes
5. Custom asset paths
6. Error handling patterns

## Performance Considerations

- Large meshes are loaded synchronously - consider using async loading for production
- Materials are copied into the library (not referenced)
- Meshes use move semantics where possible
- Index data is converted to uint32 (may increase memory for uint8/uint16 sources)

## Dependencies

- **tinygltf**: Header-only glTF 2.0 loader (included in 3rdparty)
- **nlohmann/json**: JSON parsing (bundled with tinygltf)
- **stb_image**: Image loading (bundled with tinygltf)

These are configured automatically through CMake.

## File Locations

- Header: `source/Render/Assetloader.hxx`
- Implementation: `source/Render/Assetloader.cxx`
- Examples: `source/Render/AssetLoaderExamples.cxx`
- tinygltf: `3rdparty/tinygltf/`

## CMake Integration

The AssetLoader is automatically compiled and linked. Ensure your CMakeLists.txt includes:

```cmake
target_link_libraries(${PROJECT_NAME} PRIVATE
    tinygltf
    # ... other libraries
)
```

This is already configured in the root `CMakeLists.txt`.
