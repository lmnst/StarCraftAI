cmake_minimum_required(VERSION 3.12)
project(HumanCommander LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# The original game needs the native BWAPI 4.4.0 library (not the OpenBW BWAPILIB).
set(BWAPI_INCLUDE "" CACHE PATH "BWAPI 4.4.0 include directory (contains BWAPI.h)")
set(BWAPI_LIB "" CACHE FILEPATH "Native BWAPI 4.4.0 BWAPI.lib, Win32 Release")
set(BWEM_DIR "" CACHE PATH "BWEM root (contains BWEM/src and BWEM/include)")
foreach(required BWAPI_INCLUDE BWAPI_LIB BWEM_DIR)
  if(NOT ${required})
    message(FATAL_ERROR "Pass -D${required}=<path>; see README.md")
  endif()
endforeach()

file(GLOB BWEM_SOURCES "${BWEM_DIR}/BWEM/src/*.cpp")
add_library(Candidate SHARED Candidate.def src/Bot.cpp ${BWEM_SOURCES})
target_compile_definitions(Candidate PRIVATE NOMINMAX WIN32 NDEBUG _CRT_SECURE_NO_WARNINGS)
target_compile_options(Candidate PRIVATE /bigobj)
target_include_directories(Candidate PRIVATE "${BWAPI_INCLUDE}" "${BWEM_DIR}/BWEM/include")
target_link_libraries(Candidate "${BWAPI_LIB}")

# Replay player module: records the replay frame count for audit, issues no commands.
add_library(ReplayAudit SHARED src/ReplayAudit.cpp Candidate.def)
target_include_directories(ReplayAudit PRIVATE "${BWAPI_INCLUDE}")
target_link_libraries(ReplayAudit "${BWAPI_LIB}")
