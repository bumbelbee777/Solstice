# -----------------------------------------------------------------------------
# Solstice tests — in-house harness (TestHarness.hxx / Fuzz.hxx), CTest labels
#
# Environment knobs (optional):
#   SOLSTICE_TEST_VERBOSE=1       — verbose PASS lines from harness macros
#   SOLSTICE_STRESS_TORTURE=1     — larger defaults for ECS/physics/net/script stress tests
#   SOLSTICE_ECS_STRESS_ENTITIES  — override entity count (ECSStressTest)
#   SOLSTICE_ECS_STRESS_ROUNDS    — override rounds (ECSStressTest)
#   SOLSTICE_PHYSICS_STRESS_BODIES / SOLSTICE_PHYSICS_STRESS_STEPS
#   SOLSTICE_NET_STRESS_CYCLES / SOLSTICE_NET_STRESS_PACKETS
#   SOLSTICE_SCRIPT_STRESS_ROUNDS
#   SOLSTICE_LIFECYCLE_ROUNDS
#   SOLSTICE_FUZZ_ITERATIONS / SOLSTICE_FUZZ_SEED / SOLSTICE_FUZZ_TORTURE (ParserFuzzTest)
#   NSSOLVER_TORTURE, NSSOLVER_BENCH_DEEP — see NSSolverTest.cxx
#   SOLSTICE_GPU_TEST=1           — run GpuSmokeTest real init (otherwise skipped success)
#   ENABLE_THREAD_SANITIZER       — CMake option (Clang/GCC) for ThreadSafetyTest
# -----------------------------------------------------------------------------

# CTest is enabled from the root CMakeLists.txt (enable_testing).

# Solstice V1 C API smoke test (link + optional GetProcAddress on Windows)
function(solstice_copy_runtime_dlls target_name)
    if(WIN32)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:SDL3-shared>
                $<TARGET_FILE_DIR:${target_name}>
            COMMENT "Copying SDL3 runtime for ${target_name}"
        )
    endif()
endfunction()

add_library(SolsticeTestSupport INTERFACE)
target_include_directories(SolsticeTestSupport INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

set(SOLSTICE_TEST_BIN_DIR "${CMAKE_BINARY_DIR}/bin")

function(solstice_add_ctest exe_name labels)
    # Use generator expression so multi-config generators (Visual Studio) find bin/Release or bin/Debug.
    add_test(NAME ${exe_name} COMMAND "$<TARGET_FILE:${exe_name}>")
    set_tests_properties(${exe_name} PROPERTIES LABELS "${labels}")
endfunction()

add_executable(CAPISmokeTest
    CAPISmokeTest.cxx
)
target_link_libraries(CAPISmokeTest
    PRIVATE SolsticeEngine SolsticeTestSupport LibSmf
)
set_target_properties(CAPISmokeTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_TEST_BIN_DIR}"
)
solstice_copy_runtime_dlls(CAPISmokeTest)

# Thread Safety Test
add_executable(ThreadSafetyTest
    ThreadSafetyTest.cxx
)

target_link_libraries(ThreadSafetyTest
    PRIVATE SolsticeEngine SolsticeTestSupport
)

set_target_properties(ThreadSafetyTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_TEST_BIN_DIR}"
)
solstice_copy_runtime_dlls(ThreadSafetyTest)

# Thread sanitizer support (if available)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    option(ENABLE_THREAD_SANITIZER "Enable ThreadSanitizer for thread safety tests" OFF)
    if(ENABLE_THREAD_SANITIZER)
        target_compile_options(ThreadSafetyTest PRIVATE
            -fsanitize=thread
            -g
        )
        target_link_options(ThreadSafetyTest PRIVATE
            -fsanitize=thread
        )
        message(STATUS "ThreadSanitizer enabled for ThreadSafetyTest")
    endif()
endif()

# MSVC static analysis (if available)
if(MSVC)
    target_compile_options(ThreadSafetyTest PRIVATE
        /analyze
    )
endif()

# Math Safety Test
add_executable(MathSafetyTest
    MathSafetyTest.cxx
)
target_link_libraries(MathSafetyTest
    PRIVATE SolsticeEngine SolsticeTestSupport
)
set_target_properties(MathSafetyTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_TEST_BIN_DIR}"
)
solstice_copy_runtime_dlls(MathSafetyTest)

# Job System Stress Test
add_executable(JobSystemStressTest
    JobSystemStressTest.cxx
)
target_link_libraries(JobSystemStressTest
    PRIVATE SolsticeEngine SolsticeTestSupport
)
set_target_properties(JobSystemStressTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_TEST_BIN_DIR}"
)
solstice_copy_runtime_dlls(JobSystemStressTest)

# Navier-Stokes Solver Test
add_executable(NSSolverTest
    NSSolverTest.cxx
)
target_link_libraries(NSSolverTest
    PRIVATE SolsticeEngine SolsticeTestSupport
)
set_target_properties(NSSolverTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_TEST_BIN_DIR}"
)
solstice_copy_runtime_dlls(NSSolverTest)

# Networking Test
add_executable(NetworkingTest
    NetworkingTest.cxx
)
target_link_libraries(NetworkingTest
    PRIVATE SolsticeEngine SolsticeTestSupport
)
set_target_properties(NetworkingTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_TEST_BIN_DIR}"
)
solstice_copy_runtime_dlls(NetworkingTest)

# ECS Registry Test
add_executable(ECSRegistryTest
    ECSRegistryTest.cxx
)
target_link_libraries(ECSRegistryTest
    PRIVATE SolsticeEngine SolsticeTestSupport
)
set_target_properties(ECSRegistryTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_TEST_BIN_DIR}"
)
solstice_copy_runtime_dlls(ECSRegistryTest)

# Profiler API stability test
add_executable(ProfilerTest
    ProfilerTest.cxx
)
target_link_libraries(ProfilerTest
    PRIVATE SolsticeEngine SolsticeTestSupport
)
set_target_properties(ProfilerTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_TEST_BIN_DIR}"
)
solstice_copy_runtime_dlls(ProfilerTest)

# Moonwalk typed interop + JIT behavior test
add_executable(MoonwalkInteropTest
    MoonwalkInteropTest.cxx
)
target_link_libraries(MoonwalkInteropTest
    PRIVATE SolsticeEngine SolsticeTestSupport
)
set_target_properties(MoonwalkInteropTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_TEST_BIN_DIR}"
)
solstice_copy_runtime_dlls(MoonwalkInteropTest)

# Narrative JSON/YAML round-trip + validation
add_executable(NarrativeDocumentTest
    NarrativeDocumentTest.cxx
)
target_include_directories(NarrativeDocumentTest PRIVATE "${CMAKE_SOURCE_DIR}/source")
target_link_libraries(NarrativeDocumentTest
    PRIVATE SolsticeEngine SolsticeTestSupport
)
set_target_properties(NarrativeDocumentTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_TEST_BIN_DIR}"
)
solstice_copy_runtime_dlls(NarrativeDocumentTest)

# --- Stress / pipeline tests (harness) ---

add_executable(ECSStressTest ECSStressTest.cxx)
target_link_libraries(ECSStressTest PRIVATE SolsticeEngine SolsticeTestSupport)
set_target_properties(ECSStressTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_TEST_BIN_DIR}")
solstice_copy_runtime_dlls(ECSStressTest)

add_executable(PhysicsStressTest PhysicsStressTest.cxx)
target_link_libraries(PhysicsStressTest PRIVATE SolsticeEngine SolsticeTestSupport)
set_target_properties(PhysicsStressTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_TEST_BIN_DIR}")
solstice_copy_runtime_dlls(PhysicsStressTest)

add_executable(AssetPipelineTest AssetPipelineTest.cxx)
target_compile_definitions(AssetPipelineTest PRIVATE
    "SOLSTICE_TEST_FIXTURES_DIR=\"${CMAKE_SOURCE_DIR}/tests/fixtures\""
)
target_link_libraries(AssetPipelineTest PRIVATE SolsticeEngine SolsticeTestSupport)
set_target_properties(AssetPipelineTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_TEST_BIN_DIR}")
solstice_copy_runtime_dlls(AssetPipelineTest)

add_executable(ParserFuzzTest ParserFuzzTest.cxx)
target_compile_definitions(ParserFuzzTest PRIVATE
    "SOLSTICE_TEST_FIXTURES_DIR=\"${CMAKE_SOURCE_DIR}/tests/fixtures\""
)
target_link_libraries(ParserFuzzTest PRIVATE SolsticeEngine SolsticeTestSupport)
set_target_properties(ParserFuzzTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_TEST_BIN_DIR}")
solstice_copy_runtime_dlls(ParserFuzzTest)

add_executable(NetworkingStressTest NetworkingStressTest.cxx)
target_link_libraries(NetworkingStressTest PRIVATE SolsticeEngine SolsticeTestSupport)
set_target_properties(NetworkingStressTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_TEST_BIN_DIR}")
solstice_copy_runtime_dlls(NetworkingStressTest)

add_executable(ScriptStressTest ScriptStressTest.cxx)
target_link_libraries(ScriptStressTest PRIVATE SolsticeEngine SolsticeTestSupport)
set_target_properties(ScriptStressTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_TEST_BIN_DIR}")
solstice_copy_runtime_dlls(ScriptStressTest)

add_executable(EngineLifecycleTest EngineLifecycleTest.cxx)
target_link_libraries(EngineLifecycleTest PRIVATE SolsticeEngine SolsticeTestSupport)
set_target_properties(EngineLifecycleTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_TEST_BIN_DIR}")
solstice_copy_runtime_dlls(EngineLifecycleTest)

add_executable(GpuSmokeTest GpuSmokeTest.cxx)
target_link_libraries(GpuSmokeTest PRIVATE SDL3-shared bgfx)
set_target_properties(GpuSmokeTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_TEST_BIN_DIR}")
solstice_copy_runtime_dlls(GpuSmokeTest)

add_executable(ParallaxFormatTest ParallaxFormatTest.cxx)
target_link_libraries(ParallaxFormatTest PRIVATE LibParallax SolsticeTestSupport)
set_target_properties(ParallaxFormatTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_TEST_BIN_DIR}")

add_executable(SmfFormatTest SmfFormatTest.cxx)
target_link_libraries(SmfFormatTest PRIVATE LibSmf SolsticeTestSupport)
set_target_properties(SmfFormatTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_TEST_BIN_DIR}")

if(TARGET Multimedia)
    find_program(SOLSTICE_FFMPEG_CLI NAMES ffmpeg ffmpeg.exe)
    set(SOLSTICE_LAVFI_TEST_MP4 "${CMAKE_BINARY_DIR}/test_support/solstice_lavfi_pattern.mp4")
    if(SOLSTICE_FFMPEG_CLI)
        add_custom_command(
            OUTPUT "${SOLSTICE_LAVFI_TEST_MP4}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/test_support"
            COMMAND
                "${SOLSTICE_FFMPEG_CLI}" -nostdin -hide_banner -loglevel error -y -f lavfi
                -i testsrc=duration=0.2:size=64x48:rate=10 -pix_fmt yuv420p -c:v libx264
                "${SOLSTICE_LAVFI_TEST_MP4}"
            COMMENT "Generating solstice_lavfi_pattern.mp4 for MultimediaFfmpegTest"
            VERBATIM
        )
        add_custom_target(solstice_lavfi_testclip DEPENDS "${SOLSTICE_LAVFI_TEST_MP4}")

        add_executable(MultimediaFfmpegTest MultimediaFfmpegTest.cxx)
        target_link_libraries(MultimediaFfmpegTest PRIVATE Multimedia SolsticeTestSupport)
        set_target_properties(MultimediaFfmpegTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_TEST_BIN_DIR}")
        add_dependencies(MultimediaFfmpegTest solstice_lavfi_testclip)
    endif()
endif()

# -----------------------------------------------------------------------------
# CTest registration (labels: quick, stress, network, fuzz, gpu, api)
# Run:  ctest --test-dir build -L quick
#        ctest --test-dir build -L stress -L network
# -----------------------------------------------------------------------------

solstice_add_ctest(CAPISmokeTest "quick;api")
solstice_add_ctest(ThreadSafetyTest "quick")
solstice_add_ctest(MathSafetyTest "quick")
solstice_add_ctest(JobSystemStressTest "stress")
solstice_add_ctest(NSSolverTest "quick")
solstice_add_ctest(NetworkingTest "quick;network")
solstice_add_ctest(ECSRegistryTest "quick")
solstice_add_ctest(ProfilerTest "quick")
solstice_add_ctest(MoonwalkInteropTest "quick")
solstice_add_ctest(NarrativeDocumentTest "quick")

solstice_add_ctest(ECSStressTest "quick;stress")
solstice_add_ctest(PhysicsStressTest "quick;stress")
solstice_add_ctest(AssetPipelineTest "quick")
solstice_add_ctest(ParserFuzzTest "fuzz")
solstice_add_ctest(NetworkingStressTest "stress;network")
solstice_add_ctest(ScriptStressTest "quick;stress")
solstice_add_ctest(EngineLifecycleTest "quick")
solstice_add_ctest(GpuSmokeTest "gpu")
solstice_add_ctest(ParallaxFormatTest "quick")
solstice_add_ctest(SmfFormatTest "quick")
if(TARGET MultimediaFfmpegTest)
    add_test(
        NAME MultimediaFfmpegTest
        COMMAND "$<TARGET_FILE:MultimediaFfmpegTest>" "${SOLSTICE_LAVFI_TEST_MP4}"
    )
    set_tests_properties(MultimediaFfmpegTest PROPERTIES LABELS "quick")
endif()

# Aggregate target for phased CI: cmake --build build --target solstice_quick_tests
# (depends on every executable used by ctest -L quick; omit GpuSmokeTest — label gpu only.)
add_custom_target(solstice_quick_tests COMMENT "CTest quick-label test executables")
set(_solstice_quick_test_targets
    CAPISmokeTest
    ThreadSafetyTest
    MathSafetyTest
    NSSolverTest
    NetworkingTest
    ECSRegistryTest
    ProfilerTest
    MoonwalkInteropTest
    NarrativeDocumentTest
    ECSStressTest
    PhysicsStressTest
    AssetPipelineTest
    ScriptStressTest
    EngineLifecycleTest
    ParallaxFormatTest
    SmfFormatTest
)
foreach(_qt IN LISTS _solstice_quick_test_targets)
    if(TARGET ${_qt})
        add_dependencies(solstice_quick_tests ${_qt})
    endif()
endforeach()
if(TARGET MultimediaFfmpegTest)
    add_dependencies(solstice_quick_tests MultimediaFfmpegTest)
endif()
