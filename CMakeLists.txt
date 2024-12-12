cmake_minimum_required(VERSION 3.26.4)

project(CppChess LANGUAGES CXX VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Define source files
file(GLOB SOURCES "src/*.cpp" "src/MoveGenerator/*.cpp")

# Define include directories
include_directories(include)

# Create the executable
add_executable(main ${SOURCES})
