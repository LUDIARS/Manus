add_library(manus_test_main STATIC test_main.cpp)
target_include_directories(manus_test_main PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
manus_target(manus_test_main)

set(MANUS_FIXTURES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/fixtures)

# manus_add_test(<name> <libraries...>): tests/<name>_tests.cpp -> ctest "manus.<name>".
function(manus_add_test name)
  add_executable(manus_${name}_tests ${name}_tests.cpp)
  target_link_libraries(manus_${name}_tests PRIVATE manus_test_main ${ARGN})
  target_compile_definitions(manus_${name}_tests PRIVATE MANUS_FIXTURES_DIR="${MANUS_FIXTURES_DIR}")
  manus_target(manus_${name}_tests)
  add_test(NAME manus.${name} COMMAND manus_${name}_tests)
  set_tests_properties(manus.${name} PROPERTIES TIMEOUT 120)
endfunction()

manus_add_test(util manus_core)
manus_add_test(url manus_core)
manus_add_test(config manus_core manus_io)
manus_add_test(log manus_core manus_io)
manus_add_test(server manus_app manus_boost)
manus_add_test(pairing manus_core manus_io)
manus_add_test(api manus_core)
manus_add_test(jis_vk_map manus_core)
manus_add_test(keyboard manus_core)
manus_add_test(input_plan manus_core)
# The rules around the one SendInput call, with the call replaced by a fake (every platform).
manus_add_test(batch_dispatch manus_core)
if(WIN32)
  # SendInput replaced by a fake; runs on Windows only (本社).
  manus_add_test(win32_input_sink manus_core manus_io)
endif()
manus_add_test(protocol manus_core)
manus_add_test(session manus_core)
manus_add_test(app manus_app manus_boost)
manus_add_test(static_files manus_core manus_io)
# Compatibility with the Node implementation (fixtures/golden, made by tools/record-golden.ts).
manus_add_test(golden manus_app manus_boost)

# ---- Lorica is applied (ctest "manus.lorica_applied.*"). Builds can pass while Lorica
# is not in effect (the include dropped, a hardening flag cancelled, the force-include
# lost); these tests fail then.
#
# 1. From inside a translation unit built through manus_target(): the force-included
#    lorica/unsafe_api.hpp, the standard library checks and FORTIFY level Lorica sets,
#    and Lorica::boundary's checked_add / input_budget at run time. Once per profile.
# Lorica sets FORTIFY in optimized configurations on Linux (none on Windows).
# A global property so manus_write_lorica_expectations(), run from native/, reads it too.
set_property(GLOBAL PROPERTY MANUS_LORICA_EXPECT_FORTIFY
  "$<IF:$<AND:$<CONFIG:Release,RelWithDebInfo,MinSizeRel>,$<NOT:$<BOOL:${WIN32}>>>,3,0>")
get_property(MANUS_LORICA_EXPECT_FORTIFY GLOBAL PROPERTY MANUS_LORICA_EXPECT_FORTIFY)
foreach(profile baseline extended)
  set(test_target manus_lorica_applied_${profile}_tests)
  add_executable(${test_target} lorica_applied_tests.cpp)
  target_link_libraries(${test_target} PRIVATE manus_test_main)
  if(profile STREQUAL "extended")
    manus_target(${test_target} SERVER)
    set(expect_extended "$<BOOL:${MANUS_HARDEN_EXTENDED}>")
  else()
    manus_target(${test_target})
    set(expect_extended 0)
  endif()
  target_compile_definitions(${test_target} PRIVATE
    MANUS_LORICA_EXPECT_EXTENDED=${expect_extended}
    MANUS_LORICA_EXPECT_FORTIFY=${MANUS_LORICA_EXPECT_FORTIFY})
  add_test(NAME manus.lorica_applied.${profile} COMMAND ${test_target})
  set_tests_properties(manus.lorica_applied.${profile} PROPERTIES TIMEOUT 60)
endforeach()
if(NOT MANUS_HARDEN_EXTENDED)
  # Reported as not run, not as passed: there is no EXTENDED target to look at.
  message(STATUS "Manus: manus.lorica_applied.extended disabled: MANUS_HARDEN_EXTENDED=OFF")
  set_tests_properties(manus.lorica_applied.extended PROPERTIES DISABLED TRUE)
endif()

# 2. Every Manus compile command (compile_commands.json): Lorica's include directory,
#    its flags for the target's profile, the force-include, and no cancelling flag.
#    The expectations are written once every target exists (end of native/CMakeLists.txt).
function(manus_write_lorica_expectations)
  set(out "${CMAKE_BINARY_DIR}/lorica_applied")
  get_property(targets GLOBAL PROPERTY MANUS_LORICA_TARGETS)
  # EXTENDED is expected of the resident server and every Manus library it links,
  # read from the link graph (not from manus_target's SERVER keyword, so dropping
  # the keyword is caught too), and of manus_lorica_applied_extended_tests.
  set(extended_targets "")
  if(MANUS_HARDEN_EXTENDED)
    set(pending manus)
    set(seen "")
    while(pending)
      list(POP_FRONT pending current)
      if(current IN_LIST seen OR NOT TARGET ${current})
        continue()
      endif()
      list(APPEND seen ${current})
      if(current IN_LIST targets OR current STREQUAL "manus")
        list(APPEND extended_targets ${current})
      endif()
      get_target_property(links ${current} LINK_LIBRARIES)
      if(links)
        list(APPEND pending ${links})
      endif()
    endwhile()
    list(APPEND extended_targets manus_lorica_applied_extended_tests)
  endif()
  get_property(header GLOBAL PROPERTY LORICA_UNSAFE_API_HEADER)
  get_property(expect_fortify GLOBAL PROPERTY MANUS_LORICA_EXPECT_FORTIFY)
  cmake_path(GET header PARENT_PATH include_dir)
  cmake_path(GET include_dir PARENT_PATH include_dir)
  set(not_checked "")
  if(MSVC AND WIN32)
    set(toolchain msvc)
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
    set(toolchain gnu)
  else()
    # Lorica fails configuration elsewhere (no unhardened build), so this is not reached today.
    set(toolchain other)
    set(not_checked "Lorica has no hardening profile for ${CMAKE_SYSTEM_NAME} ${CMAKE_CXX_COMPILER_ID}")
  endif()
  if(NOT CMAKE_GENERATOR MATCHES "^(Ninja|Ninja Multi-Config|Unix Makefiles)$")
    set(not_checked "the ${CMAKE_GENERATOR} generator writes no compile_commands.json")
  elseif(NOT CMAKE_EXPORT_COMPILE_COMMANDS)
    set(not_checked "CMAKE_EXPORT_COMPILE_COMMANDS is OFF (Manus is not the top-level project)")
  endif()
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    set(control_flow -mbranch-protection=standard)
  else()
    set(control_flow -fcf-protection=full)
  endif()
  file(GENERATE OUTPUT "${out}/expect-$<CONFIG>.cmake" CONTENT "\
set(MANUS_COMPILE_COMMANDS [[${CMAKE_BINARY_DIR}/compile_commands.json]])
set(MANUS_SOURCE_DIR [[${PROJECT_SOURCE_DIR}]])
set(MANUS_BINARY_DIR [[${CMAKE_BINARY_DIR}]])
set(MANUS_NOT_CHECKED [[${not_checked}]])
set(MANUS_TOOLCHAIN ${toolchain})
set(MANUS_TARGETS ${targets})
set(MANUS_EXTENDED_TARGETS ${extended_targets})
set(MANUS_LORICA_HEADER [[${header}]])
set(MANUS_LORICA_INCLUDE_DIR [[${include_dir}]])
set(MANUS_CONTROL_FLOW ${control_flow})
set(MANUS_EXPECT_FORTIFY ${expect_fortify})
")
endfunction()
add_test(NAME manus.lorica_applied.compile_commands COMMAND ${CMAKE_COMMAND}
  "-DMANUS_EXPECT_FILE=${CMAKE_BINARY_DIR}/lorica_applied/expect-$<CONFIG>.cmake"
  -P ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_lorica_compile_commands.cmake)
# A generator without compile_commands.json is reported as skipped, never as passed.
set_tests_properties(manus.lorica_applied.compile_commands PROPERTIES
  TIMEOUT 120 SKIP_REGULAR_EXPRESSION "MANUS_LORICA_NOT_CHECKED:")

# 3. The resident server's executable (Lorica's tools/verify_binary.py), when Python is there.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  set(verify_args "")
  if(CMAKE_READELF)
    list(APPEND verify_args --readelf ${CMAKE_READELF})
  endif()
  add_test(NAME manus.lorica_applied.binary COMMAND ${Python3_EXECUTABLE} -I
    ${MANUS_LORICA_DIR}/tools/verify_binary.py ${verify_args} $<TARGET_FILE:manus>)
  set_tests_properties(manus.lorica_applied.binary PROPERTIES TIMEOUT 60)
else()
  message(STATUS "Manus: manus.lorica_applied.binary not registered: python3 not found")
endif()
