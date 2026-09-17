add_gtest(CarryUnit
  SOURCES CarryUnit.cpp
  LIBRARIES
    carry::carry
    project_options
    project_sanitizers
    project_coverage
)
