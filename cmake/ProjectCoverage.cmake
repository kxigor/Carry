# cmake/ProjectCoverage.cmake

# ========================================
# 1. CODE COVERAGE ARCHITECTURE SETUP
# ========================================
# This module defines the architecture necessary for measuring code coverage
# using GCC/Clang (GCOV/LCOV format).
# 
# It creates the 'project_coverage' INTERFACE library to conditionally apply 
# coverage flags only when the CURRY_ENABLE_COVERAGE option is ON. This allows 
# linking against 'project_coverage' without affecting the build when coverage 
# is disabled (the library becomes a no-op).

include_guard()

# Defines an interface target to carry coverage properties.
add_library(project_coverage INTERFACE)

# ========================================
# 2. CONDITIONAL FLAG APPLICATION & TOOLS
# ========================================
if(CURRY_ENABLE_COVERAGE)
  # Use the standard GCC/Clang flag which enables both instrumentation and 
  # branch coverage features (-fprofile-arcs, -ftest-coverage).
  set(COVERAGE_FLAGS "--coverage")

  # Apply compilation flags:
  # The flags are applied only for C++ (CXX) and only for GNU/Clang compilers.
  target_compile_options(project_coverage INTERFACE
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:${COVERAGE_FLAGS}>
  )

  # Apply linking flags (required to link GCOV libraries):
  target_link_options(project_coverage INTERFACE
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:${COVERAGE_FLAGS}>
  )

  # Search for necessary LCOV tools
  find_program(LCOV_EXECUTABLE NAMES lcov)
  find_program(GENHTML_EXECUTABLE NAMES genhtml)

  # ========================================
  # 3. CUSTOM CTEST TARGET REGISTRATION
  # ========================================
  if(LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
    message(STATUS "Found LCOV/GenHTML. Coverage target will be registered.")
    
    set(LCOV_OUTPUT_DIR "${CMAKE_BINARY_DIR}/coverage_report")
    set(LCOV_INFO_FILE "${LCOV_OUTPUT_DIR}/coverage.info")

    add_custom_target(coverage
      COMMAND ${CMAKE_COMMAND} -E make_directory ${LCOV_OUTPUT_DIR}
      COMMAND ${LCOV_EXECUTABLE} --zerocounters --directory ${CMAKE_BINARY_DIR}
      COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --test-dir ${CMAKE_BINARY_DIR}
      COMMAND ${LCOV_EXECUTABLE} --capture --directory ${CMAKE_BINARY_DIR} --output-file ${LCOV_INFO_FILE}
      COMMAND ${LCOV_EXECUTABLE} --remove ${LCOV_INFO_FILE} '/usr/*' '*/third_party/*' -o ${LCOV_INFO_FILE}
      COMMAND ${GENHTML_EXECUTABLE} ${LCOV_INFO_FILE} --output-directory ${LCOV_OUTPUT_DIR}
      COMMENT "Running tests and generating LCOV HTML report in ${LCOV_OUTPUT_DIR}"
      USES_TERMINAL
    )
  else()
    message(WARNING "LCOV/GenHTML tools not found. Coverage target skipped.")
  endif()
endif()