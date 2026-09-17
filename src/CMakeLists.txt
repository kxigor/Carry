# =========================================================
# carry — header-only library target
# =========================================================
# No compiled sources: the whole library lives in include/carry.hpp.
# Consumers (tests, app, downstream projects) link `carry::carry` to pull in
# the include path and the required C++ standard.

add_library(carry INTERFACE)
add_library(carry::carry ALIAS carry)

target_include_directories(carry INTERFACE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_compile_features(carry INTERFACE cxx_std_23)
