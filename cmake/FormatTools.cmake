# cmake/FormatTools.cmake

# ========================================
# 1. CLANG-FORMAT TOOL SEARCH
# ========================================
# This module is responsible for finding the Clang-Format executable necessary
# to run the 'FormatCheck' CTest target.
# 
# The search is performed silently, and the status/logging messages are 
# presented here to the user, based on the outcome of the search.
# This variable is intended to be used by the main MYPROJ_ENABLE_FORMAT_CHECK logic.

include_guard()

message(STATUS "Searching for code formatting tool: clang-format...")

# --- Clang-Format ---
find_program(CLANG_FORMAT_EXE
  NAMES 
    clang-format-24
    clang-format-23
    clang-format-22
    clang-format-21
    clang-format-20
    clang-format-19
  DOC "Path to the preferred version of clang-format for formatting."
)

# ========================================
# 2. TOOL STATUS LOGGING
# ========================================
# The CLANG_FORMAT_EXE variable will be used by the registration logic 
# in tests/CMakeLists.txt to conditionally create the CTest target.

if(CLANG_FORMAT_EXE)
  message(STATUS "Clang-Format found: ${CLANG_FORMAT_EXE}. FormatCheck target is enabled.")
else()
  # If the tool is not found, the main logic should disable the MYPROJ_ENABLE_FORMAT_CHECK option.
  message(WARNING "Clang-Format not found. FormatCheck target will be disabled.")
endif()