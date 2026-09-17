cmake_minimum_required(VERSION 3.25)

# =========================================================
# RENAMING THIS TEMPLATE
# =========================================================
# Search & replace the two placeholder tokens in this repository:
#
#   carry  →  your-project-name        (used in project() and paths)
#   MYPROJ                →  YOUR_PROJECT_UPPER       (option/cache prefix, uppercase)
#
# Files touched: CMakeLists.txt, CMakePresets.json, tests/CMakeLists.txt,
#                tests/static/CMakeLists.txt, cmake/ProjectCoverage.cmake,
#                README.md.
#
# A one-liner if you trust sed (run from the repo root):
#   NEW=myproject; NEW_UPPER=$(echo "$NEW" | tr 'a-z-' 'A-Z_'); \
#     git ls-files | xargs sed -i \
#       -e "s/carry/$NEW/g" \
#       -e "s/CARRY_/${NEW_UPPER}_/g"
# =========================================================

project(carry LANGUAGES CXX) # TODO: your project name

# =========================================================
# 0. STANDALONE vs SUBPROJECT
# =========================================================
# PROJECT_IS_TOP_LEVEL is set by project() since CMake 3.21.
# When this project is consumed via add_subdirectory / FetchContent, dev-time
# machinery (app, tests, qa checks, sanitizers, coverage) stays disabled by
# default.

if(PROJECT_IS_TOP_LEVEL)
  set(CARRY_DEV_DEFAULT ON)
else()
  set(CARRY_DEV_DEFAULT OFF)
endif()

# =========================================================
# 1. OPTIONS (prefix with CARRY_ — rename to keep the cache name unique)
# =========================================================
option(CARRY_BUILD_APP           "Build the example app"             ${CARRY_DEV_DEFAULT})
option(CARRY_BUILD_TESTS         "Build the GTest test suite"        ${CARRY_DEV_DEFAULT})
option(CARRY_ENABLE_FORMAT_CHECK "Register clang-format as CTest"    ${CARRY_DEV_DEFAULT})
option(CARRY_ENABLE_TIDY_CHECK   "Register clang-tidy as CTest"      ${CARRY_DEV_DEFAULT})
option(CARRY_ENABLE_COVERAGE     "Enable gcov/lcov coverage targets" OFF)

# Format/tidy/coverage only make sense alongside tests.
if(NOT CARRY_BUILD_TESTS)
  set(CARRY_ENABLE_FORMAT_CHECK OFF CACHE BOOL "Disabled: requires CARRY_BUILD_TESTS=ON" FORCE)
  set(CARRY_ENABLE_TIDY_CHECK   OFF CACHE BOOL "Disabled: requires CARRY_BUILD_TESTS=ON" FORCE)
  set(CARRY_ENABLE_COVERAGE     OFF CACHE BOOL "Disabled: requires CARRY_BUILD_TESTS=ON" FORCE)
endif()

# =========================================================
# 2. TOP-LEVEL-ONLY GLOBAL SETTINGS
# =========================================================
# These touch *global* CMake state. When this project is a subproject, the
# parent decides build type, exports and the C++ standard.

if(PROJECT_IS_TOP_LEVEL)
  if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
  endif()
  set(CMAKE_CXX_STANDARD 23)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

# =========================================================
# 3. SUBDIRECTORIES
# =========================================================
add_subdirectory(src)

# =========================================================
# 4. DEV-ONLY MACHINERY (skipped for subproject consumers)
# =========================================================
if(CARRY_BUILD_APP OR CARRY_BUILD_TESTS OR CARRY_ENABLE_COVERAGE)
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
  include(ProjectOptions)
  include(ProjectSanitizers)
  include(ProjectCoverage)
endif()

if(CARRY_BUILD_APP)
  add_subdirectory(app)
endif()

if(CARRY_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
