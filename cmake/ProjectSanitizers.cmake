# cmake/ProjectSanitizers.cmake

# ========================================
# 1. ARCHITECTURE SETUP & PURPOSE
# ========================================
# This module defines the build configuration for enabling specialized 
# Compiler Sanitizers (AddressSanitizer, ThreadSanitizer, etc.). 
# Sanitizers are crucial for finding memory errors, thread safety issues, 
# and undefined behavior during development and testing.
#
# It creates the 'project_sanitizers' INTERFACE library, which is meant to be 
# linked by targets that require instrumentation. The flags are applied 
# conditionally based on the SANITIZER_MODE option.

include_guard()

# The interface library carries all compiler and linker properties for sanitization.
add_library(project_sanitizers INTERFACE)

# ========================================
# 2. MODE CONFIGURATION & USER OPTIONS
# ========================================

# SANITIZER_MODE allows the user to select the active sanitizer via configuration.
# Note: Only one sanitizer can be active at a time (e.g., ASAN and TSAN are mutually exclusive).
set(SANITIZER_MODE "Address" CACHE STRING "Sanitizer mode: None, Address, Thread, Memory, Undefined")
set_property(CACHE SANITIZER_MODE PROPERTY STRINGS "None" "Address" "Thread" "Memory" "Undefined")

# ========================================
# 3. SANITIZER PRESETS (FLAGS)
# ========================================

# --- GNU/CLANG (Common flags for GCC and Clang) ---

# --- UBSAN (Undefined Behavior Sanitizer) ---
# UBSAN is often included with other sanitizers as it catches many basic errors.
set(UBSAN_FLAGS
  -fsanitize=undefined
  -fno-sanitize-recover=undefined # Forces immediate crash on error
  -fno-omit-frame-pointer)        # Required for proper stack traces

# --- ASAN (Address Sanitizer) + UBSAN ---
set(ASAN_FLAGS
  -fsanitize=address
  ${UBSAN_FLAGS})

# --- TSAN (Thread Sanitizer) + UBSAN ---
set(TSAN_FLAGS
  -fsanitize=thread
  ${UBSAN_FLAGS})

# --- MSAN (Memory Sanitizer) + UBSAN ---
set(MSAN_FLAGS
  -fsanitize=memory
  -fsanitize-memory-track-origins # Tracks where uninitialized data came from
  ${UBSAN_FLAGS})

# --- MSVC (Visual Studio) ---

# --- ASAN (Address Sanitizer) ---
set(MSVC_ASAN_FLAGS
  /fsanitize=address)

# ========================================
# 4. APPLYING LOGIC VIA GENERATOR EXPRESSIONS
# ========================================
# Flags are only applied if the mode is NOT "None". The appropriate flag set 
# is selected based on the SANITIZER_MODE and the compiler ID.

if(NOT SANITIZER_MODE STREQUAL "None")
  # Use list to append all conditional flags based on the selected mode
  list(APPEND SAN_APPLIED_FLAGS
    # --- ASAN (Address Sanitizer) ---
    $<$<AND:$<STREQUAL:${SANITIZER_MODE},Address>,$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>>:${ASAN_FLAGS}>
    $<$<AND:$<STREQUAL:${SANITIZER_MODE},Address>,$<CXX_COMPILER_ID:MSVC>>:${MSVC_ASAN_FLAGS}>

    # --- TSAN (Thread Sanitizer) ---
    $<$<AND:$<STREQUAL:${SANITIZER_MODE},Thread>,$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>>:${TSAN_FLAGS}>

    # --- MSAN (Memory Sanitizer) ---
    $<$<AND:$<STREQUAL:${SANITIZER_MODE},Memory>,$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>>:${MSAN_FLAGS}>

    # --- UBSAN (Undefined Behavior Sanitizer) ---
    $<$<AND:$<STREQUAL:${SANITIZER_MODE},Undefined>,$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>>:${UBSAN_FLAGS}>
  )

  # Apply selected flags to the interface target (compilation and linking)
  target_compile_options(project_sanitizers INTERFACE ${SAN_APPLIED_FLAGS})
  target_link_options(project_sanitizers INTERFACE ${SAN_APPLIED_FLAGS})
endif()