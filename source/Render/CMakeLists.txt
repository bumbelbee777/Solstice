# Render: BGFX pipeline, scene, camera, post-processing, etc.
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_library(Render STATIC
    FluidVolumeVisualizer.cxx
    Scene/Camera.cxx
    Context/Context.cxx
    Framebuffers/Framebuffer.cxx
    Framebuffers/FramebufferRenderTarget.cxx
    SoftwareRenderer.cxx
    Post/PostProcessing.cxx
    Lighting/Raytracing.cxx
    Lighting/VolumetricLighting.cxx
    Scene/Skybox.cxx
    Scene/AuthoringSkyboxApply.cxx
    Assets/ShaderLoader.cxx
    Particle/ParticleSystem.cxx
    Particle/ParticlePresets.cxx
    Assets/TextureRegistry.cxx
    Context/RenderPipeline.cxx
    Scene/SceneRenderer.cxx
    Lighting/ShadowRenderer.cxx
    Framebuffers/FramebufferManager.cxx
    Post/HDRProcessor.cxx
    Assets/ShaderUniforms.cxx
)
target_include_directories(Render PUBLIC ${SOURCE_DIR})
target_link_libraries(Render PUBLIC Core Math Arzachel Physics bimg bx)
target_link_libraries(Render PRIVATE Plugin)
if(MSVC)
    target_compile_options(Render PRIVATE /Zc:preprocessor /W0)
    target_compile_definitions(Render PRIVATE SOLSTICE_EXPORTS _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()
