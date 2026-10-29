cmake_minimum_required(VERSION 3.24)
project(Manus VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
include(CTest)

option(MANUS_WARNINGS_AS_ERRORS "Treat compiler warnings in Manus sources as errors" OFF)

find_package(OpenSSL 3 REQUIRED)
find_package(Threads REQUIRED)

# Dependencies are pinned by URL hash (same archives as Odeum/native). Pass
# -DFETCHCONTENT_SOURCE_DIR_BOOST=<dir> / -DFETCHCONTENT_SOURCE_DIR_JSON=<dir> to use a local copy.
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d)
# Header-only use of Asio/Beast: SOURCE_SUBDIR names a directory that does not exist,
# so Boost's own CMake build is never configured.
FetchContent_Declare(boost
  URL https://github.com/boostorg/boost/releases/download/boost-1.87.0/boost-1.87.0-b2-nodocs.zip
  URL_HASH SHA256=40efbb1f38b8cf186fd7bb12809f561ae1c233bae0f75146862e9bfce77d0d42
  SOURCE_SUBDIR manus-header-only)
FetchContent_MakeAvailable(json boost)

# Lorica: a local checkout (LORICA_SOURCE_DIR) or the pinned commit.
set(LORICA_SOURCE_DIR "" CACHE PATH "Local Lorica checkout; empty fetches the pinned commit")
set(LORICA_BUILD_PROTECTION OFF CACHE BOOL "" FORCE)
set(LORICA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
if(LORICA_SOURCE_DIR)
  add_subdirectory(${LORICA_SOURCE_DIR} ${CMAKE_BINARY_DIR}/_deps/lorica-build EXCLUDE_FROM_ALL)
else()
  FetchContent_Declare(lorica
    GIT_REPOSITORY https://github.com/LUDIARS/Lorica
    GIT_TAG cab4f12086e30484f249d1b4aa479bf7cdec13a9)
  FetchContent_MakeAvailable(lorica)
endif()

add_library(manus_boost INTERFACE)
target_include_directories(manus_boost SYSTEM INTERFACE ${boost_SOURCE_DIR})
target_compile_definitions(manus_boost INTERFACE BOOST_ERROR_CODE_HEADER_ONLY BOOST_ASIO_NO_DEPRECATED)
target_link_libraries(manus_boost INTERFACE Threads::Threads)
if(WIN32)
  target_link_libraries(manus_boost INTERFACE ws2_32 mswsock)
endif()

# Warning set applied to every Manus target (not to third-party headers, which are SYSTEM).
add_library(manus_warnings INTERFACE)
if(MSVC)
  target_compile_options(manus_warnings INTERFACE /utf-8 /W4 /permissive- /external:W0)
  target_compile_definitions(manus_warnings INTERFACE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
  if(MANUS_WARNINGS_AS_ERRORS)
    target_compile_options(manus_warnings INTERFACE /WX)
  endif()
else()
  target_compile_options(manus_warnings INTERFACE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow)
  if(MANUS_WARNINGS_AS_ERRORS)
    target_compile_options(manus_warnings INTERFACE -Werror)
  endif()
endif()

# Every locally compiled target goes through this: warnings + Lorica hardening.
function(manus_target target)
  target_link_libraries(${target} PRIVATE manus_warnings)
  lorica_harden_target(${target})
endfunction()

add_subdirectory(src)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
