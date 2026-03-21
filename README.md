# **Solstice Engine**

*A CPU-centric game engine for retro-modern projects.*

![Physics Demo](media/physics_demo.png)

## **Overview**

**Solstice** is a custom engine designed to achieve **PS2 → early PS3–level visuals** and gameplay entirely on the CPU (with optional iGPU assist). It combines **classic retro constraints** (baked lighting, compact textures, BSPs) with **modern HPC techniques** (SoA/AoSoA memory layouts, SIMD, async batching, bitmask acceleration).

**Target platforms:** Modern CPUs (8+ cores) at 480p–720p (1080p tech-demo mode).  
**Target use cases:** Stylized 3D games, experimental renderers, or "next-gen retro" projects.

## **Philosophy & Objectives**

**Solstice** is not a GPU replacement — it's a deliberate throwback reimagined with modern CPU horsepower. The engine follows three core principles:

1. **Retro spirit, modern efficiency.** Limit resolution, texture sizes, and dynamic lights — then claw back performance with HPC-style memory layouts, SIMD, and batching.
2. **Hybrid raster + bitplane rays.** Use rasterization for pixels, bitwise tricks for shadows/visibility/AO.
3. **No wasted cycles.** Zero-copy, cache-friendly layouts, async batching, and aggressive culling keep CPU cores busy without thrashing memory.

By carefully constraining scope and exploiting CPU parallelism, Solstice achieves a sweet spot: visuals far beyond PS2, comparable to early PS3 launch titles, while staying lightweight and scalable on today's processors.

## **Implemented Features**

### **🎨 Rendering System**

#### **Core Rendering**
- ✅ **Tile-based software rasterizer** (8×8 or 16×16 tiles) with SIMD-optimized inner loops
- ✅ **Fixed-point edge functions** and depth interpolation for precision
- ✅ **Early-Z rejection** for efficient depth testing
- ✅ **SIMD-accelerated attribute interpolation** (UVs, normals, colors)
- ✅ **G-buffer lite:** Albedo, normal (oct-encoded), material ID, depth
- ✅ **SceneRenderer** for main scene rendering
- ✅ **ShadowRenderer** for dynamic shadow mapping
- ✅ **Post-processing pipeline** with HDR support
- ✅ **Skybox rendering** with cubemap support
- ✅ **Procedural texture generation** (checkerboard, noise, stripes, etc.)
- ✅ **Physics debug mode:** visualize collision shapes and physics bodies
- ✅ **Object selection system:** select and highlight objects in the scene
- ✅ **Hover detection:** detect and highlight objects under cursor
- ✅ **Wireframe rendering:** toggle wireframe mode for debugging
- ✅ **VSync control:** enable/disable vertical synchronization
- ✅ **Render statistics:** detailed performance metrics (triangles, cull time, etc.)
- ✅ **Optimization flags:** static buffer optimization, async rendering thresholds

#### **Scene Management**
- ✅ **BSP (Binary Space Partitioning)** for spatial partitioning
- ✅ **Octree** for hierarchical scene organization
- ✅ **SIMD frustum culling** for efficient visibility testing
- ✅ **Software occlusion culling** with low-res Hi-Z buffer
- ✅ **Aggressive multi-tier LOD system** (geometry, texture, feature toggling)
- ✅ **SoA/AoSoA data layouts** for transforms, AABBs, vertex attributes (SIMD-friendly)
- ✅ **Scene graph** with mesh and material libraries
- ✅ **Camera system** with perspective and orthographic projections

#### **Lighting & Shading**
- ✅ **Baked lightmaps** (RGBM encoding) + ambient probes
- ✅ **Dynamic lights:** 1–4 small point/spot lights blended per tile
- ✅ **Shadow mapping** for dynamic shadows
- ✅ **Static shadows:** baked into lightmaps
- ✅ **Shading model:** Lambert diffuse + Blinn-Phong specular
- ✅ **Normal mapping** support
- ✅ **PBR materials** (metallic/roughness workflow + some physical properties like conductiveness, buoyancy, and flammability)

#### **Bitplane Raytracing (Bitwise Wizardry)**
- ✅ **Voxel/Cluster bitmasks** for fast visibility and shadow checks
- ✅ **Bitwise ray marching:** rays represented as masks; traversal = shifts & AND ops
- ✅ **Packet rays:** groups of rays tested together with bitmask propagation
- ✅ **Shadow rays** for cheap shadow computation
- ✅ **Ambient occlusion approximation** via bitmask queries
- ✅ **Portal/visibility queries** using bitplane traversal

#### **Post Processing**
- ✅ **Upscaling:** internal 480p/540p → output 720p (FSR-like edge-aware upscale)
- ✅ **Temporal AA (TAA)** + jittered reprojection for stability
- ✅ **Bloom** effect
- ✅ **Tone mapping** (HDR to LDR conversion)
- ✅ **Fog** rendering
- ✅ **HDR processing** pipeline
- ✅ **Motion Blur:** velocity buffer-based motion blur with quality presets (Low/Medium/High)

#### **Particle Systems**
- ✅ **Base particle system** with customizable particle types
- ✅ **Billboard rendering** for particle quads
- ✅ **Distance-based culling** for performance optimization
- ✅ **Configurable spawn rates** and density controls
- ✅ **Extensible architecture** for custom particle behaviors

#### **Asset Loading**
- ✅ **glTF 2.0 support** (.gltf and .glb formats)
- ✅ **Mesh loading:** positions, normals, UVs (TEXCOORD_0)
- ✅ **Extended vertex data:** vertex colors (RGBA), tangents (XYZW with handedness)
- ✅ **Index buffers:** all component types (uint8, uint16, uint32)
- ✅ **Multiple primitives:** submeshes with different materials
- ✅ **PBR materials:** base color, roughness, metallic, emission
- ✅ **Texture references** for materials
- ✅ **Scene hierarchy:** node structure with transforms
- ✅ **Camera data loading:** perspective and orthographic cameras from glTF
- ✅ **Light data loading:** point, spot, and directional lights (KHR_lights_punctual extension)
- ✅ **glTF export:** export meshes, materials, and scene data to glTF format
- ✅ **Wavefront OBJ parser** for legacy mesh formats
- ✅ **Asset caching** with hash-based deduplication
- ✅ **Asset path management:** centralized asset path resolution
- ✅ **Memory-mapped streaming:** zero-copy asset loading for large files
- ✅ **Async asset loading:** non-blocking asset loading with futures

### **⚙️ Physics & Collision**

#### **Collision Detection**
- ✅ **GJK (Gilbert-Johnson-Keerthi)** algorithm for convex collision detection
- ✅ **CCD (Continuous Collision Detection)** for fast-moving objects
- ✅ **BVH (Bounding Volume Hierarchy)** for efficient broadphase
- ✅ **Bitmask broadphase:** world quantized into voxel/bin grids with bitmask occupancy
- ✅ **SIMD batch AABB tests** for object culling
- ✅ **Contact generation** and clustering
- ✅ **Manifold management** for persistent contacts
- ✅ **Convex hull factory** for generating collision shapes

#### **Physics Simulation**
- ✅ **Rigid body dynamics** with ReactPhysics3D integration
- ✅ **Iterative solver** for constraint resolution
- ✅ **Collision resolution** with friction and restitution
- ✅ **Shape helpers** for common primitives (box, sphere, capsule, etc.)
- ✅ **Physics system** with timestep management

#### **Advanced Physics**
- ✅ **Fluid simulation** with Navier-Stokes solver (NSSolver)
- ✅ **Light sources** as physics objects for dynamic lighting
- ✅ **Physics-to-scene synchronization** for rendering

### **🧠 Core Systems**

#### **Concurrency & Memory**
- ✅ **Job system** with lock-free queues and per-core allocators
- ✅ **Async batching** for parallel rendering tasks
- ✅ **Zero-copy asset streaming** with memory-mapped files
- ✅ **Frame allocator** for per-frame temporary allocations
- ✅ **Custom allocators** for efficient memory management
- ✅ **Cache system** for asset and data caching

#### **Spatial Data Structures**
- ✅ **BSP (Binary Space Partitioning)** for scene partitioning
- ✅ **Octree** for hierarchical spatial queries
- ✅ **NavMesh** with A* and Theta* pathfinding algorithms

#### **AI & Machine Learning**
- ✅ **MLP (Multi-Layer Perceptron)** neural networks
- ✅ **CNN (Convolutional Neural Network)** with SIMD-optimized kernels
- ✅ **Activation functions:** ReLU, Tanh, Sigmoid, SIREN
- ✅ **Quantization support:** INT8, INT4 for model compression
- ✅ **Functional weight generation** using SIREN (periodic sine functions)

#### **State Management**
- ✅ **FSM (Finite State Machine)** for game logic and AI states
- ✅ **State transitions** with condition functions and events

#### **Profiling System**
- ✅ **Frame timing:** per-frame CPU timing and FPS tracking
- ✅ **Scope timing:** hierarchical scope-based performance measurement
- ✅ **Thread-level profiling:** per-thread scope tracking and statistics
- ✅ **GPU timing:** GPU scope timing with BGFX integration
- ✅ **Performance counters:** custom counter tracking for game metrics
- ✅ **Memory tracking:** allocation and peak memory monitoring
- ✅ **Call graphs:** hierarchical call graph visualization
- ✅ **Flame graph export:** speedscope-compatible JSON export
- ✅ **Performance warnings:** configurable thresholds for frame time spikes and memory leaks
- ✅ **ImGui overlay:** real-time profiler visualization

#### **Utilities**
- ✅ **Debug logging system** with log levels
- ✅ **Audio system** with SDL_mixer integration
- ✅ **SIMD utilities** for vectorized operations
- ✅ **Asset streaming** with async loading support
- ✅ **Save system** with save/load functionality
- ✅ **Save file validation** with CRC32 checksum
- ✅ **Save file repair** for corrupted saves
- ✅ **Backup/restore** functionality for save files

### **📜 Scripting System (Moonwalk)**

- ✅ **Bytecode VM:** stack-based virtual machine with 16 registers
- ✅ **Script compiler** for converting scripts to bytecode
- ✅ **Hot reloading:** runtime script reloading without restart
- ✅ **Module system:** import/export for code organization
- ✅ **Debugging support:** breakpoints, step-through execution, call stack inspection
- ✅ **Bytecode optimization:** constant folding, dead code elimination, register allocation
- ✅ **Enhanced error handling:** stack traces, detailed exception classes, source line mapping
- ✅ **JIT compilation:** architecture-specific backends (x64, ARM64, RISC-V) with hot path detection
- ✅ **Native function bindings** for comprehensive engine integration
- ✅ **Script bindings** for:
  - **ECS:** Entity Component System operations
  - **Math:** vectors, matrices, quaternions, trigonometric, hyperbolic, exponential/logarithmic functions
  - **Physics:** rigid bodies, collisions, force/impulse, velocity, constraints, gravity
  - **Renderer:** scene management, camera, lighting, materials, post-processing
  - **Arzachel:** procedural mesh generation, operations, transforms, materials, animation
  - **UI:** complete ImGui widget library, layout management, menus, popups, tables, drag-and-drop
  - **Profiler:** scope profiling, counters, memory tracking, frame statistics
- ✅ **Collection types:** Array, Dictionary, Set
- ✅ **Value system:** supports int64, double, string, Vec2/3/4, Matrix2/3/4, Quaternion, collections
- ✅ **Language enhancements:** iterator-based for loops, enhanced operators, data structures, functions

### **🎮 Entity Component System (ECS)**

- ✅ **Registry** for entity and component management
- ✅ **Component storage** with type-safe access
- ✅ **Transform system** for 3D transformations
- ✅ **Entity ID system** for unique entity identification
- ✅ **Component types:** Transform, Name, Kind, PlayerTag, CurrentPlayer
- ✅ **System architecture** for game logic organization

### **🖼️ UI System**

#### **Core UI Infrastructure**
- ✅ **Window management** with SDL3 integration
- ✅ **UI system** for UI state management
- ✅ **ImGui integration** for immediate-mode GUI
- ✅ **UIRenderSettings:** customizable rendering settings system for different UI layers
  - Separate view IDs for HUD and UI rendering
  - Point filtering for pixel-perfect rendering
  - Blending and depth test configuration
  - Factory methods for HUD and default settings
- ✅ **Widget system** for custom UI components
- ✅ **Event handling** for input and window events
- ✅ **ViewportUI** for in-game UI overlays
- ✅ **Direct HUD rendering:** `ImGui::GetBackgroundDrawList()` for pixel-perfect HUD elements

#### **Animation System**
- ✅ **Keyframe-based animation:** template-based keyframe system with time-based interpolation
- ✅ **Animation tracks:** separate tracks for position, scale, rotation, color, and alpha
- ✅ **Animation clips:** multi-property animation containers with synchronized tracks
- ✅ **Animation player:** play, pause, stop, seek, and loop control
- ✅ **Easing functions:** 12+ easing types (Linear, EaseIn/Out/InOut, Bounce, Elastic, Back, Circ, Expo, Quad, Cubic, Quart, Quint, Bezier)
- ✅ **Custom bezier curves:** configurable cubic bezier easing
- ✅ **Type-safe interpolation:** automatic interpolation for ImVec2, ImVec4, floats, and custom types

#### **Motion Graphics**
- ✅ **Animated widgets:** buttons, text, and images with animation clips
- ✅ **Transitioning widgets:** windows and panels with fade, slide, scale, and rotate transitions
- ✅ **World-space animations:** animated dialogs, labels, and buttons in 3D space
- ✅ **Sprite rendering:** 2D and world-space sprite rendering with texture support
- ✅ **Animated primitives:** polygons and gradients with time-based animations
- ✅ **Visual effects integration:** animated glow and shadow effects

#### **Image Loading & Sprites**
- ✅ **Image loader:** singleton image loading system with caching
- ✅ **Multi-format support:** STB and BIMG image loaders
- ✅ **Memory loading:** load images from memory buffers
- ✅ **Texture management:** automatic texture caching and cleanup
- ✅ **Image info queries:** get image dimensions and format without full load
- ✅ **Sprite system:** Sprite class for 2D and world-space rendering
- ✅ **Sprite sheet:** grid-based sprite sheet loading and frame management
- ✅ **Texture atlas:** texture atlas with named sprite lookup
- ✅ **Frame animation:** sprite frame duration and animation support

#### **Drawing Primitives**
- ✅ **Shapes:** polygons, stars (filled and outlined), bezier curves (quadratic and cubic), arcs, pie slices
- ✅ **Gradients:** linear, radial, and conic gradients
- ✅ **Patterns:** stripe, dot, and checkerboard patterns
- ✅ **World-space rendering:** primitives rendered in 3D viewport space
- ✅ **Customizable styling:** colors, thickness, segments, and angles

#### **Transitions**
- ✅ **Fade transitions:** alpha-based fade in/out
- ✅ **Slide transitions:** position-based sliding animations
- ✅ **Scale transitions:** size-based scaling animations
- ✅ **Rotate transitions:** rotation-based animations
- ✅ **Combined transitions:** multiple transitions running simultaneously
- ✅ **Easing support:** all transitions support easing functions
- ✅ **State management:** idle, running, paused, and completed states
- ✅ **Callbacks:** on-start and on-complete event callbacks

#### **Visual Effects**
- ✅ **Shadow effects:** drop shadow, inner shadow, colored shadow, glow, multiple shadows
- ✅ **Blur effects:** Gaussian, box, motion, and background blur
- ✅ **World-space effects:** shadows and effects in 3D space
- ✅ **Combined effects:** widgets with multiple visual effects
- ✅ **Backdrop blur:** frosted glass effect for transparency
- ✅ **Animated effects:** time-based animated blur and shadow effects

#### **Text & Transparency**
- ✅ **Text effects:** Shake, Bounce, Rotate, Scale, Wave, Glow animations
- ✅ **Transparency system:** layered UI rendering with alpha blending
- ✅ **Animated transparency:** time-based transparency animations

### **🔢 Math Library**

- ✅ **Vector math:** Vec2, Vec3, Vec4 with SIMD support
- ✅ **Matrix math:** Matrix2, Matrix3, Matrix4
- ✅ **Quaternion** for rotations
- ✅ **SIMD-optimized Vec3** for performance-critical operations
- ✅ **Geometric operations:** dot product, cross product, normalization, etc.

### **🎭 Arzachel Procedural Generation System**

The **Arzachel** subsystem provides a functional, deterministic procedural generation system for meshes, textures, and animations.

#### **Generator System**
- ✅ **Seed-based deterministic generation:** same seed always produces same result
- ✅ **Functional programming model:** Generator<T> with map, flatMap, and combine operations
- ✅ **Monadic composition:** chain generators with derived seeds to avoid correlation
- ✅ **Pure functions:** all generators are stateless and deterministic

#### **Procedural Mesh Generation**
- ✅ **Polyhedra primitives:** Cube, Sphere, Cylinder, Torus, Icosphere, Plane
- ✅ **Geometry operations:**
  - **Merge:** combine multiple meshes
  - **Transform:** apply matrix transformations
  - **Instance:** create multiple instances with different transforms
  - **CSG operations:** Union, Difference, Intersection (simplified implementations)

#### **Rigged Mesh System**
- ✅ **Skeleton system:** hierarchical bone structures with BoneID references
- ✅ **Skin weights:** vertex-to-bone weight mapping with multi-bone support
- ✅ **Rigged mesh primitives:**
  - **RigidRoot:** single root bone for static meshes
  - **Chain:** linear chain of bones for simple hierarchies
  - **Spine:** segmented chain for flexible structures
- ✅ **Rigging operations:**
  - **MergeSkeletons:** combine two skeletons deterministically
  - **RemapWeights:** remap bone references in skin weights
  - **MergeRigged:** combine rigged meshes with skeleton merging

#### **Animation System**
- ✅ **AnimationClip:** immutable animation containers with duration
- ✅ **AnimationTrack:** per-bone animation tracks with pattern matching
- ✅ **Keyframe system:** translation, rotation, and scale keyframes
- ✅ **Pose evaluation:** evaluate animations at any time point
- ✅ **Bone transforms:** translation, rotation (quaternion), and scale
- ✅ **World transform calculation:** accumulates parent transforms
- ✅ **Animation operations:** blending, sequencing, and manipulation utilities

#### **Procedural Textures**
- ✅ **Noise textures:** Perlin noise with configurable octaves and scale
- ✅ **Pattern textures:** checkerboard, stripes with customizable colors
- ✅ **Material textures:** roughness maps, normal maps, etc.
- ✅ **Deterministic generation:** seed-based for reproducible results

#### **Asset Import/Export**
- ✅ **glTF mesh import:** convert glTF meshes to MeshData generators
- ✅ **glTF rigged import:** import meshes with skeleton and skin weights
- ✅ **glTF animation import:** parse animation channels and samplers
- ✅ **glTF export:** export procedural meshes to glTF format

### **🎮 Game Systems**

#### **Game Framework**
- ✅ **GameBase:** base class for game applications with update/render loop
- ✅ **Frame rate control:** configurable max FPS with automatic throttling
- ✅ **Game time tracking:** delta time and total game time
- ✅ **Window integration:** seamless SDL3 window management

#### **State Management**
- ✅ **GameState system:** finite state machine for game states (MainMenu, Playing, Paused, etc.)
- ✅ **State transitions:** conditional transitions with callback support
- ✅ **State callbacks:** enter, exit, and update callbacks per state
- ✅ **State persistence:** save and load game state

#### **Player Controllers**
- ✅ **FirstPersonController:** FPS-style movement with mouse look
  - Move speed, sprint multiplier, jump force
  - Crouch mechanics with height adjustment
  - Head bob and camera shake effects
  - Ground detection and physics integration
- ✅ **ThirdPersonController:** third-person camera with orbit controls
  - Camera distance and angle controls
  - Smooth camera following
  - Collision-aware camera positioning

#### **Combat & Health**
- ✅ **Health system:**
  - Current/max health and armor values
  - Health regeneration with configurable rate and delay
  - Invincibility frames after damage
  - Death callbacks and event system
- ✅ **Damage events:** temporary components for damage processing
- ✅ **Heal events:** temporary components for healing
- ✅ **Weapon system:** comprehensive weapon management
  - Multiple weapon types (Pistol, Rifle, Shotgun, Sniper, Melee, etc.)
  - Ammo management (current, max, reserve)
  - Fire rate and reload timing
  - Spread/accuracy system with decay
  - Hitscan and projectile support
  - Weapon switching system
- ✅ **Stamina system:** stamina management for sprinting and actions

#### **AI Systems**
- ✅ **Enemy system:** AI with multiple states
  - **States:** Idle, Patrol, Chase, Attack, Flee, Dead
  - Aggro range and attack range detection
  - Patrol radius and waypoint navigation
  - Vision cone and hearing range
  - Attack cooldown and damage values
  - Drop table system for loot
- ✅ **Boss system:** extended enemy system for boss encounters
  - Multiple attack phases
  - Special attack patterns
  - Health threshold triggers
- ✅ **AIBrain system:** advanced AI decision-making system
- ✅ **NPC system:** non-player character management and interactions

#### **Gameplay Systems**
- ✅ **Inventory system:** item management with slots and categories
- ✅ **HUD system:** comprehensive heads-up display
  - Direct rendering using `ImGui::GetBackgroundDrawList()` for pixel-perfect rendering
  - UIRenderSettings integration for separate HUD view (View ID 11)
  - Health bar with numeric values
  - Inventory display
  - Minimap rendering
  - Crosshair rendering
  - Ammo/weapon display
  - Stamina bar
  - Speed indicator
  - Equipped weapon display
  - Objective display
  - Damage indicators (world-space floating damage numbers)
- ✅ **InputManager:** centralized input handling with key/button mapping
- ✅ **SceneManager:** scene loading and management
- ✅ **ComponentFactory:** factory pattern for creating game components
- ✅ **GamePreferences:** persistent game settings and preferences
- ✅ **Autosave system:** automatic save game management
- ✅ **LevelSelector:** level selection and progression system
- ✅ **Difficulty system:** configurable difficulty levels
- ✅ **SFXManager:** sound effects management and playback
- ✅ **LoadingScreen:** loading screen system with progress tracking
- ✅ **PauseMenu:** enhanced pause menu system
  - Toggle functionality
  - Settings state tracking and integration
  - Background alpha customization
  - Menu callbacks (Resume, Settings, Save, Quit)
- ✅ **MainMenu:** enhanced main menu system
  - Level selector integration for level selection
  - Settings menu integration
  - Load game menu with save slot management
  - Color customization (primary, secondary, accent colors)
  - Subtitle support
  - Menu callbacks (NewGame, LoadGame, Settings, Quit)

### **🎬 Animation & Rigging**

- ✅ **Skeleton system:** hierarchical bone structures
- ✅ **Bone transforms:** translation, rotation (quaternion), scale
- ✅ **Skin weights:** multi-bone vertex weighting
- ✅ **Animation clips:** time-based animation containers
- ✅ **Animation tracks:** per-bone animation channels
- ✅ **Keyframe interpolation:** smooth animation between keyframes
- ✅ **Pose evaluation:** evaluate animations at arbitrary time points
- ✅ **World space transforms:** automatic parent transform accumulation

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
* **Object selection:** visual highlighting of selected objects
* **Hover detection:** hover state for interactive objects
* **Physics debug visualization:** wireframe rendering of physics shapes
* **Async rendering:** parallel tile rasterization with configurable thresholds

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
* **Volumetric lighting:** with god rays and light occlusion.

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
* **Textures:** ≤1024×1024, with mipmaps.
* **Materials:** compact (≤40B) per entry.

## **Performance Targets**

* **720p60** for "PS2.5 → early PS3-lite" visuals.
* **1080p30** for stress/tech demos.
* Geometry budget: ~20–50k visible triangles per frame.
* Lighting budget: 1 sun, a few dynamic lights, baked lightmaps.
* Physics budget: ~1–2ms per frame for gameplay collisions.

## **Documentation**

Comprehensive documentation is available in the `docs/` directory:

- **[ECS.md](docs/ECS.md)**: Entity Component System architecture and API
- **[Renderer.md](docs/Renderer.md)**: Rendering system, pipeline, and APIs
- **[Physics.md](docs/Physics.md)**: Physics system, collision detection, and integration
- **[UI.md](docs/UI.md)**: UI system, UIRenderSettings, widgets, and viewport UI
- **[MotionGraphics.md](docs/MotionGraphics.md)**: Animation, transitions, visual effects, and sprites
- **[Scripting.md](docs/Scripting.md)**: BytecodeVM, compiler, hot reloading, and script bindings
- **[GameLayer.md](docs/GameLayer.md)**: Game systems, menus, HUD, and gameplay components
- **[Arzachel.md](docs/Arzachel.md)**: Procedural generation system for meshes, textures, and materials

## **Example Projects**

The engine includes several example projects demonstrating its capabilities:

- **PhysicsPlayground:** Interactive physics demo with object spawning, selection, and manipulation
- **Maze:** Procedural maze generation and navigation
- **Blizzard:** Weather and particle effects demonstration with snow particle systems
- **Hyperbourne:** FPS game example showcasing weapon systems, HUD, and combat mechanics

## **Building**

### Prerequisites

- **CMake** 3.20 or higher
- **C++20** compatible compiler (MSVC 2019+, GCC 10+, Clang 12+)
- **Python 3** (for shader compilation)
- **Git** (for fetching dependencies via CPM)

### Building from Source

Dependencies (BGFX, SDL3, ImGui, etc.) are fetched automatically at configure time via [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake). No manual clone of 3rdparty or submodules is required.

#### Linux/macOS quickstart scripts

Use the helper scripts to install prerequisites and run a preset-based build:

On Linux, `tools/setup_env_linux.sh` supports `apt-get`, `dnf`, and `pacman` (Arch Linux).

```bash
# Linux
bash tools/setup_env_linux.sh
bash tools/build_linux.sh
# or release build:
bash tools/build_linux.sh --release
```

```bash
# macOS
bash tools/setup_env_macos.sh
bash tools/build_macos.sh
# or clean release build:
bash tools/build_macos.sh --release --clean
```

The scripts wrap the same CMake preset flow shown below. You can always use manual commands directly if preferred.
They also perform post-build runtime setup by creating expected directories (for example `cache`, `bin/assets`, `bin/assets/fonts`) and staging `assets/` and compiled shaders when available.

```bash
# Clone the repository (no --recursive needed)
git clone https://github.com/bumbelbee777/Solstice.git
cd Solstice

# Configure with CMake (CPM will download dependencies on first run)
cmake --preset default

# Build
cmake --build out/build/default
```

**Optional:** To cache downloads across builds (e.g. for CI or multiple configs), set `CPM_SOURCE_CACHE` before configuring:

```bash
# Use a shared cache directory (default is .cpm-cache in the project root)
cmake -DCPM_SOURCE_CACHE="$HOME/.cache/cpm" --preset default
```

Or set the environment variable `CPM_SOURCE_CACHE` to a path of your choice.

## **Dependencies**

All dependencies are managed by CPM and fetched at configure time:

- **BGFX** (bx, bimg, bgfx): Cross-platform rendering library
- **SDL3:** Window and input management
- **SDL_mixer:** Audio playback
- **ReactPhysics3D:** Physics simulation
- **ImGui:** Immediate-mode GUI
- **tinygltf:** glTF 2.0 asset loading
- **nlohmann/json:** (vendored by tinygltf) JSON parsing

## **License**

See LICENSE file for details.

## **Contributing**

Contributions are welcome! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

---

**Solstice** — Where retro meets modern CPU power.
