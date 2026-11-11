# Fuzz entries (libFuzzer's LLVMFuzzerTestOneInput) for every parser of outside
# input. Each entry is built twice:
#   - manus_fuzz_<name>_regression (every compiler, ctest "manus.fuzz.<name>"):
#     runs the entry once on each seed in corpus/<name>/ without libFuzzer;
#   - manus_fuzz_<name> (MANUS_BUILD_FUZZERS=ON, Clang only): the libFuzzer binary.
set(MANUS_FUZZ_CORPUS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/corpus)

function(manus_add_fuzz name)
  if(BUILD_TESTING)
    add_executable(manus_fuzz_${name}_regression fuzz_${name}.cpp regression_main.cpp)
    target_link_libraries(manus_fuzz_${name}_regression PRIVATE ${ARGN})
    manus_target(manus_fuzz_${name}_regression)
    add_test(NAME manus.fuzz.${name} COMMAND manus_fuzz_${name}_regression ${MANUS_FUZZ_CORPUS_DIR}/${name})
    set_tests_properties(manus.fuzz.${name} PROPERTIES TIMEOUT 120)
  endif()
  if(MANUS_BUILD_FUZZERS)
    add_executable(manus_fuzz_${name} fuzz_${name}.cpp)
    target_link_libraries(manus_fuzz_${name} PRIVATE ${ARGN})
    target_link_options(manus_fuzz_${name} PRIVATE -fsanitize=fuzzer)
    manus_target(manus_fuzz_${name})
  endif()
endfunction()

manus_add_fuzz(ws_message manus_core)
manus_add_fuzz(pair_body manus_core)
manus_add_fuzz(url manus_core)
manus_add_fuzz(config manus_core)
manus_add_fuzz(http_request manus_core manus_boost)
