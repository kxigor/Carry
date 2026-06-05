add_gtest(CurryUnit
  SOURCES CurryUnit.cpp
  LIBRARIES
    curry::curry
    project_options
    project_sanitizers
    project_coverage
)
