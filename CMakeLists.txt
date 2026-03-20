cmake_minimum_required(VERSION 3.14)
project(border-queue-optimizer
    VERSION 1.0.0
    LANGUAGES C
    DESCRIPTION "Queue management and optimization engine for customs border crossings"
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Compiler warnings
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wdouble-promotion)
endif()

# ── Library: core engine ─────────────────────────────────────────────
set(CORE_SOURCES
    src/heap.c
    src/stats.c
    src/queue_model.c
    src/simulator.c
    src/optimizer.c
    src/predictor.c
    src/arrival.c
)

add_library(bqo_core STATIC ${CORE_SOURCES})
target_include_directories(bqo_core PUBLIC include src)
target_link_libraries(bqo_core PUBLIC m)

# ── Executable: CLI ──────────────────────────────────────────────────
add_executable(border-queue-optimizer src/main.c)
target_link_libraries(border-queue-optimizer PRIVATE bqo_core)

# ── Tests ────────────────────────────────────────────────────────────
enable_testing()

add_executable(bqo_tests
    tests/test_main.c
    tests/test_queue_model.c
    tests/test_heap.c
    tests/test_simulator.c
    tests/test_optimizer.c
)
target_link_libraries(bqo_tests PRIVATE bqo_core)

add_test(NAME unit_tests COMMAND bqo_tests)

# ── Install ──────────────────────────────────────────────────────────
install(TARGETS border-queue-optimizer RUNTIME DESTINATION bin)
