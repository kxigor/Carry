# cmake/ProjectOptions.cmake

# ========================================
# 1. ARCHITECTURE SETUP & PURPOSE
# ========================================
# This module defines the core compiler flags, optimization levels, and
# hardening flags for the entire project. It is essential for C++ standard 
# enforcement, code quality (warnings), and security (stack protection).
#
# It creates the 'project_options' INTERFACE library, which is meant to be 
# linked by ALL project targets (executables, libraries) to ensure consistent 
# build settings across the entire codebase.

include_guard()

# The interface library carries all compiler and linker properties.
add_library(project_options INTERFACE)

# ========================================
# 2. COMPILER PRESETS (COMPILE OPTIONS)
# ========================================

# --- GCC/Clang Compiler ---
# Base flags: Essential warnings, security hardening (PIE, stack protection).
set(GCC_BASE_COMPILE
  -Wall -Wextra -Wpedantic # Standard, recommended warnings
  -fPIE                    # Position Independent Executable (Security)
  -fstack-protector-strong # Basic Stack Overflow protection (Security)
)

# Aggressive flags: Enables maximal strictness, used primarily for Debug/CI builds
# to catch advanced conversion issues, overflow errors, and potential bugs early.
set(GCC_AGGRESSIVE_COMPILE
  -Werror -Waggressive-loop-optimizations -Wmissing-declarations -Wcast-align 
  -Wcast-qual -Wchar-subscripts -Wconversion -Wempty-body -Wformat-nonliteral 
  -Wsuggest-final-methods -Wsuggest-final-types -Wswitch-default  -Werror=vla 
  -Wsign-conversion -Wstrict-overflow=2 -Wsuggest-attribute=noreturn -Wunused
  -Wno-varargs -fcheck-new -fstrict-overflow -fno-omit-frame-pointer -Winline
  -Wvariadic-macros  -Wno-missing-field-initializers  -Wno-narrowing -Wpacked
  -Wswitch-enum   -Wsync-nand    -Wundef  -Wunreachable-code   -Wno-self-move 
  -Wformat-security      -Wformat-signedness      -Wformat=2     -Wlogical-op 
  -Wopenmp-simd       -Wpointer-arith      -Winit-self      -Wredundant-decls 
)

set(GCC_DEBUG_COMPILE -g -D_DEBUG -ggdb3 -O0)  # Debugging and no optimization
set(GCC_RELWITHDEB -O2 -g -DNDEBUG)            # Moderate optimization + Debug info
set(GCC_RELEASE -O3 -DNDEBUG)                  # Full optimization

# --- MSVC Compiler ---
# Base flags: Essential warnings, treating warnings as errors (/WX), and C++ standard enforcement.
set(MSVC_BASE_COMPILE /W4 /WX /permissive- /Zc:__cplusplus)
set(MSVC_DEBUG_COMPILE /Zi /Ob0 /Od /RTC1)      # Debug info, no optimization, runtime checks
set(MSVC_RELWITHDEB /Zi /O2 /DNDEBUG)           # Debug info, moderate optimization
set(MSVC_RELEASE /O2 /Oi /Gy /DNDEBUG /Zi /GL)  # Full optimization + Link-time code generation flags

# ========================================
# 3. LINKER PRESETS (LINK OPTIONS)
# ========================================

# --- GCC/Clang Linker ---
# Hardening flags: Position Independent Executable linking, RELRO (Read-Only relocations).
set(GCC_BASE_LINK
  -pie
  -Wl,-z,relro
  -Wl,-z,now # Full RELRO protection (lazy binding disabled)
)

# --- MSVC Linker ---
# Hardening flags: Address Space Layout Randomization (ASLR) and Data Execution Prevention (DEP).
set(MSVC_BASE_LINK
  /DYNAMICBASE
  /NXCOMPAT
)

# ========================================
# 4. APPLYING LOGIC VIA GENERATOR EXPRESSIONS
# ========================================
# All flags are applied conditionally based on the target compiler ID and the
# selected build configuration (Debug, Release, etc.).

target_compile_options(project_options INTERFACE
  # --- Base Flags (Applied to ALL configurations) ---
  $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>: ${GCC_BASE_COMPILE}>
  $<$<COMPILE_LANG_AND_ID:CXX,MSVC>: ${MSVC_BASE_COMPILE}>

  # --- Debug Configuration ---
  # Note: Aggressive warnings are included here to maximize code quality during development.
  $<$<AND:$<CONFIG:Debug>,$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>>: ${GCC_DEBUG_COMPILE} ${GCC_AGGRESSIVE_COMPILE}>
  $<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>: ${MSVC_DEBUG_COMPILE}>

  # --- RelWithDebInfo Configuration ---
  $<$<AND:$<CONFIG:RelWithDebInfo>,$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>>: ${GCC_RELWITHDEB}>
  $<$<AND:$<CONFIG:RelWithDebInfo>,$<CXX_COMPILER_ID:MSVC>>: ${MSVC_RELWITHDEB}>

  # --- Release Configuration ---
  $<$<AND:$<CONFIG:Release>,$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>>: ${GCC_RELEASE}>
  $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>: ${MSVC_RELEASE}>
)

target_link_options(project_options INTERFACE
  # --- Base Hardening Flags ---
  $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>: ${GCC_BASE_LINK}>
  $<$<COMPILE_LANG_AND_ID:CXX,MSVC>: ${MSVC_BASE_LINK}>

  # --- MSVC Release-Specific Optimization (Link Time Code Generation) ---
  $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>: /LTCG>
)