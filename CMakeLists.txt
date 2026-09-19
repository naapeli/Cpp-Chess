cmake_minimum_required(VERSION 3.15)
project(CppChess VERSION 0.2.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(CHESS_SANITIZERS "Enable address and undefined-behavior sanitizers" OFF)

add_library(chess_core
    src/utils.cpp src/Board/board.cpp
    src/MoveGenerator/AttackTables.cpp src/MoveGenerator/MoveGenerator.cpp
    src/Engine/transpositionTable.cpp src/Engine/evaluation.cpp
    src/Engine/engine.cpp src/Engine/search.cpp
    src/Engine/quiescence.cpp src/Engine/move_ordering.cpp)
target_include_directories(chess_core PUBLIC include)
if(MSVC)
    target_compile_options(chess_core PRIVATE /W4)
else()
    target_compile_options(chess_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_definitions(chess_core PUBLIC $<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS>)
endif()
if(CHESS_SANITIZERS AND NOT MSVC)
    target_compile_options(chess_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(chess_core PUBLIC -fsanitize=address,undefined)
endif()

include(CTest)
if(EMSCRIPTEN)
    # Search recursion and the test harness need more than the SDK's 64 KiB default.
    target_link_options(chess_core PUBLIC "SHELL:-s STACK_SIZE=1048576")
    add_executable(engine src/wasm_wrapper.cpp src/uci.cpp)
    target_link_libraries(engine PRIVATE chess_core)
    target_link_options(engine PRIVATE --no-entry
        "SHELL:-s EXPORTED_RUNTIME_METHODS=['ccall']"
        "SHELL:-s EXPORTED_FUNCTIONS=['_uci_command']"
        "SHELL:-s MODULARIZE=1"
        "SHELL:-s EXPORT_NAME=createChessEngine"
        "SHELL:-s ASYNCIFY=1"
        "SHELL:-s ASYNCIFY_STACK_SIZE=1048576"
        "SHELL:-s ALLOW_MEMORY_GROWTH=1"
        "SHELL:-s DISABLE_EXCEPTION_CATCHING=0")
    target_compile_options(chess_core PRIVATE -fexceptions)
    target_compile_options(engine PRIVATE -fexceptions)
    set(CHESS_WASM_OUTPUT_DIR "${CMAKE_BINARY_DIR}" CACHE PATH "Directory for generated browser engine")
    set_target_properties(engine PROPERTIES SUFFIX ".js" RUNTIME_OUTPUT_DIRECTORY "${CHESS_WASM_OUTPUT_DIR}")
    if(BUILD_TESTING)
        add_executable(engine_tests tests/engine_tests.cpp)
        target_link_libraries(engine_tests PRIVATE chess_core)
        target_compile_options(engine_tests PRIVATE -fexceptions)
        target_link_options(engine_tests PRIVATE "SHELL:-s DISABLE_EXCEPTION_CATCHING=0" "SHELL:-s ALLOW_MEMORY_GROWTH=1")
        add_test(NAME wasm_correctness COMMAND engine_tests)
        add_test(NAME wasm_api COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR}
            ${CMAKE_SOURCE_DIR}/tests/test_wasm.cjs $<TARGET_FILE:engine>)
    endif()
else()
    find_package(Threads REQUIRED)
    add_executable(engine src/main.cpp src/uci.cpp)
    target_link_libraries(engine PRIVATE chess_core Threads::Threads)
    if(BUILD_TESTING)
        add_executable(engine_tests tests/engine_tests.cpp)
        target_link_libraries(engine_tests PRIVATE chess_core)
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
        foreach(suite perft evaluation engine)
            add_test(NAME test_${suite} COMMAND ${Python3_EXECUTABLE} -m pytest -q
                ${CMAKE_SOURCE_DIR}/tests/test_${suite}.py --engine $<TARGET_FILE:engine>)
            set_tests_properties(test_${suite} PROPERTIES
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 180)
        endforeach()
    endif()
endif()
