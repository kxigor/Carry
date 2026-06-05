# =========================================================
# curry — header-only library target
# =========================================================
# No compiled sources: the whole library lives in include/curry.hpp.
# Consumers (tests, app, downstream projects) link `curry::curry` to pull in
# the include path and the required C++ standard.

add_library(curry INTERFACE)
add_library(curry::curry ALIAS curry)

target_include_directories(curry INTERFACE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_compile_features(curry INTERFACE cxx_std_23)
