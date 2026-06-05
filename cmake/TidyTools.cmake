# cmake/TidyTools.cmake

# ========================================
# 1. CLANG-TIDY AND PYTHON SEARCH
# ========================================
# This module is responsible for finding the Clang-Tidy executable and the 
# Python 3 interpreter. Both are mandatory dependencies for registering 
# and running the 'TidyCheck' CTest target, as the execution usually relies 
# on a Python wrapper script.

include_guard()

message(STATUS "Searching for static analysis tools: clang-tidy and Python3 ...")

# --- Clang-Tidy ---
find_program(CLANG_TIDY_EXE
  NAMES
    clang-tidy-24
    clang-tidy-23
    clang-tidy-22
    clang-tidy-21
    clang-tidy-20
    clang-tidy-19
  DOC "Path to the preferred version of clang-tidy for static analysis."
)

# --- Python 3 ---
find_package(Python3 QUIET)

# ========================================
# 2. STATUS AND FINAL LOGIC
# ========================================
# We check each tool separately for clear logging, and then determine the 
# final status of the TidyCheck target. If dependencies are incomplete, 
# the CLANG_TIDY_EXE variable is unset to signal the failure.

if(CLANG_TIDY_EXE)
  message(STATUS "Clang-Tidy found: ${CLANG_TIDY_EXE}")
else()
  message(WARNING "Clang-Tidy not found. Static analysis checks will be disabled.")
endif()

if(Python3_FOUND)
  message(STATUS "Python 3 found.")
else()
  message(WARNING "Python 3 (required for the analysis script) not found.")
endif()

# --- The Final Logic ---
if(CLANG_TIDY_EXE AND Python3_FOUND)
  message(STATUS "Result: TidyCheck targets are FULLY enabled.")
elseif(CLANG_TIDY_EXE AND NOT Python3_FOUND)
  message(WARNING "Result: TidyCheck targets disabled due to missing Python 3 dependency.")
  unset(CLANG_TIDY_EXE)
else()
  message(STATUS "Result: TidyCheck targets are skipped.")
  unset(CLANG_TIDY_EXE)
endif()