# cmake/GtestTools.cmake

# ========================================
# 1. GOOGLE TEST (GTEST) PREREQUISITES
# ========================================
# This module is responsible for finding the GTest framework, which is required 
# for building all unit test executables in the 'unit' subdirectory.
# It defines the 'add_gtest' function which wraps the necessary CMake and CTest 
# commands for creating a fully linked and registered test target.

include_guard()

message(STATUS "Searching for testing prerequisites: GTest ...")

# --- GTest ---
# Use CONFIG mode for finding the GTest package, which is standard practice 
# for packages that use modern CMake targets (GTest::gtest_main).
find_package(GTest QUIET CONFIG)

# --- Status of tools ---
if(GTest_FOUND)
  message(STATUS "Found GTest. Unit tests are enabled.")
else()
  message(WARNING "GTest package not found. Unit tests will be disabled.")
endif()


# ==============================================================================
# FUNCTION: add_gtest(TEST_NAME ...)
# 
# Description: Creates an executable for a Google Test suite, links it with
# GTest framework and specified dependencies, and registers it with CTest.
#
# Crucially, this function allows automatic application of essential targets 
# like 'project_options' and 'project_sanitizers' via the LIBRARIES argument
# in the calling file, ensuring consistency with the main codebase.
# 
# Requires: GTest::gtest_main to be available (i.e., GTest_FOUND must be TRUE).
# ==============================================================================
#
# --- ARGUMENTS PARSED BY cmake_parse_arguments ---
#
# OPTIONS:
#   EXCLUSIVE: Sets the CTest property RUN_SERIAL TRUE, forcing this test 
#              to run exclusively (not in parallel) with other tests.
#
# ONE_VALUE:
#   WORKING_DIRECTORY: Sets the working directory for the test execution.
#
# MULTI_VALUE:
#   SOURCES: Mandatory list of source files (e.g., test_file.cpp) for the test executable.
#   LIBRARIES: List of targets to link (e.g., MyCoreLib::Core, project_options).
#   INCLUDE_DIRECTORIES: Additional include paths for compilation.
#   COMPILE_OPTIONS: Custom compile flags (e.g., -Wno-conversion).
#   COMPILE_DEFINITIONS: Custom definitions (e.g., ENABLE_MOCKING).
#   LINK_OPTIONS: Custom linker flags.
#   PROPERTIES: Additional CTest properties passed as Key/Value pairs (e.g., TIMEOUT 60).
#
# --- USAGE EXAMPLE ---
# add_gtest(MyTestSuiteName
#   SOURCES test_file_1.cpp [test_file_2.cpp ...]
#   LIBRARIES MyCoreLib::MyCoreLib project_options project_sanitizers
#   PROPERTIES TIMEOUT 60
#   EXCLUSIVE 
# )
#
function(add_gtest TEST_NAME)
  # Pre-check: Skip the function execution if GTest was not found during configuration.
  if(NOT GTest_FOUND)
      message(STATUS "Skipping test registration: GTest not found.")
      message(STATUS "(Skipped call was in file: ${CMAKE_CURRENT_FUNCTION_LIST_FILE})")
      return()
    endif()

  cmake_parse_arguments(
    TEST_ARGS
    # OPTIONS (Flags that require no value)
    "EXCLUSIVE"
    # ONE_VALUE (Arguments requiring a single value)
    "WORKING_DIRECTORY"
    # MULTI_VALUE (Arguments that can take multiple space-separated values)
    "SOURCES;LIBRARIES;INCLUDE_DIRECTORIES;COMPILE_OPTIONS;COMPILE_DEFINITIONS;LINK_OPTIONS;PROPERTIES"
    ${ARGN}
  )

  # --- Checking the availability of sources ---
  if(NOT TEST_ARGS_SOURCES)
    message(FATAL_ERROR "add_gtest requires SOURCES to be specified for test ${TEST_NAME}")
  endif()
  
  # --- 1. Creating an executable file ---
  add_executable(${TEST_NAME} ${TEST_ARGS_SOURCES})

  # --- 2. Using compilation/linking options (if provided) ---
  if(TEST_ARGS_COMPILE_OPTIONS)
    target_compile_options(${TEST_NAME} PRIVATE ${TEST_ARGS_COMPILE_OPTIONS})
  endif()

  if(TEST_ARGS_COMPILE_DEFINITIONS)
    target_compile_definitions(${TEST_NAME} PRIVATE ${TEST_ARGS_COMPILE_DEFINITIONS})
  endif()
  
  if(TEST_ARGS_INCLUDE_DIRECTORIES)
    target_include_directories(${TEST_NAME} PRIVATE ${TEST_ARGS_INCLUDE_DIRECTORIES})
  endif()
  
  if(TEST_ARGS_LINK_OPTIONS)
    target_link_options(${TEST_NAME} PRIVATE ${TEST_ARGS_LINK_OPTIONS})
  endif()
  
  # --- 3. Linking (GTest + custom libraries) ---
  target_link_libraries(${TEST_NAME}
    PRIVATE
    GTest::gtest_main # Links the GTest framework with the default main() function
    ${TEST_ARGS_LIBRARIES} # Includes project libraries, options, and sanitizers targets
  )

  # --- 4. Registration of the CTest test ---
  add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
  
  # --- 5. Applying additional CTest options ---
  # EXCLUSIVE flag sets the CTest property RUN_SERIAL TRUE to prevent parallel execution.
  if(TEST_ARGS_EXCLUSIVE)
    set_tests_properties(${TEST_NAME} PROPERTIES RUN_SERIAL TRUE)
  endif()

  if(TEST_ARGS_WORKING_DIRECTORY)
    set_tests_properties(${TEST_NAME} PROPERTIES WORKING_DIRECTORY ${TEST_ARGS_WORKING_DIRECTORY})
  endif()

  if(TEST_ARGS_PROPERTIES)
    # PROPERTIES is passed as a K/V list (e.g., TIMEOUT 60)
    set_tests_properties(${TEST_NAME} PROPERTIES ${TEST_ARGS_PROPERTIES})
  endif()

endfunction()