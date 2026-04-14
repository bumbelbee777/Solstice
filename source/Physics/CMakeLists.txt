# Physics: ReactPhysics3D bridge, collision, fluids, etc.
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_library(Physics STATIC
    Integration/PhysicsSystem.cxx
    Integration/ReactPhysics3DBridge.cxx
    Fluid/FluidSparse.cxx
    Fluid/Fluid.cxx
    Fluid/NSSolver.cxx
    Dynamics/IterativeSolver.cxx
    Collision/Narrowphase/CollisionResolution.cxx
    Collision/Narrowphase/ShapeHelpers.cxx
    Collision/Narrowphase/ContactGeneration.cxx
    Collision/Narrowphase/ManifoldManager.cxx
    Collision/Narrowphase/ContactClustering.cxx
    Collision/Narrowphase/GJK.cxx
    Collision/Broadphase/CCD.cxx
    Content/ConvexHullFactory.cxx
    Lighting/LightSource.cxx
)
target_include_directories(Physics PUBLIC ${SOURCE_DIR})
target_link_libraries(Physics PUBLIC Core Math ReactPhysics3D::reactphysics3d)
target_link_libraries(Physics PRIVATE Plugin)
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(Physics PUBLIC OpenMP::OpenMP_CXX)
    target_compile_definitions(Physics PRIVATE SOLSTICE_HAS_OPENMP)
endif()
if(MSVC)
    target_compile_options(Physics PRIVATE /Zc:preprocessor /W0)
    target_compile_definitions(Physics PRIVATE SOLSTICE_EXPORTS _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()
