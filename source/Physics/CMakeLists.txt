# Physics: ReactPhysics3D bridge, collision, fluids, etc.
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_library(Physics STATIC
    PhysicsSystem.cxx
    ReactPhysics3DBridge.cxx
    CollisionResolution.cxx
    IterativeSolver.cxx
    ShapeHelpers.cxx
    ContactGeneration.cxx
    ManifoldManager.cxx
    ContactClustering.cxx
    ConvexHullFactory.cxx
    GJK.cxx
    CCD.cxx
    LightSource.cxx
    Fluid.cxx
    NSSolver.cxx
)
target_include_directories(Physics PUBLIC ${SOURCE_DIR})
target_link_libraries(Physics PUBLIC Core Math ReactPhysics3D::reactphysics3d)
if(MSVC)
    target_compile_options(Physics PRIVATE /Zc:preprocessor /W0)
    target_compile_definitions(Physics PRIVATE SOLSTICE_EXPORTS _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()
