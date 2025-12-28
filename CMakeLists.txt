cmake_minimum_required(VERSION 3.15)
project(WasmChessEngine CXX)

# 1. Standard Setup
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 2. Source Files
set(SOURCES
    src/utils.cpp
    src/Engine/transpositionTable.cpp
    src/MoveGenerator/AttackTables.cpp
    src/MoveGenerator/MoveGenerator.cpp
    src/Board/board.cpp
    src/Engine/engine.cpp
    src/wasm_wrapper.cpp
)

add_executable(engine ${SOURCES})

# 3. Include Directories
target_include_directories(engine PUBLIC src include)

# 4. Common Flags (Applied to BOTH Debug and Release)
# These are necessary for the engine to function in the browser regardless of optimization.
target_link_options(engine PRIVATE
    --no-entry
    "SHELL:-s EXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
    "SHELL:-s EXPORTED_FUNCTIONS=['_init_engine','_get_best_move','_make_move','_new_state']"
    "SHELL:-s WASM=1"
    "SHELL:-s ALLOW_MEMORY_GROWTH=1"
    # Keep exception catching enabled if your logic relies on it, 
    # otherwise move to Debug if you want faster Release builds.
    "SHELL:-s DISABLE_EXCEPTION_CATCHING=0" 
)

# 5. Debug-Specific Flags
# Applied only when you build with -DCMAKE_BUILD_TYPE=Debug
target_compile_options(engine PRIVATE $<$<CONFIG:Debug>:-g>)
target_link_options(engine PRIVATE $<$<CONFIG:Debug>:
    -g                                      # Generate debug symbols (readable stack traces)
    "SHELL:-s ASSERTIONS=1"                 # Enable runtime checks
    "SHELL:-s SAFE_HEAP=1"                  # (Optional) Checks for memory alignment issues
    "SHELL:-s STACK_OVERFLOW_CHECK=1"       # (Optional) Helpful for deep recursion
>)

# 6. Release-Specific Flags
# Applied only when you build with -DCMAKE_BUILD_TYPE=Release
target_compile_options(engine PRIVATE $<$<CONFIG:Release>:-O3>)
target_link_options(engine PRIVATE $<$<CONFIG:Release>:
    -O3                                     # Aggressive optimization
    "SHELL:-s ASSERTIONS=0"                 # Remove runtime checks for speed
>)

# 7. Output Configuration
# This ensures the output ALWAYS goes to /website relative to your project root,
# regardless of where you run the 'make' command from.
set_target_properties(engine PROPERTIES
    SUFFIX ".js"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/website"
)