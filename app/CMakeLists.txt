add_executable(app main.cpp) # TODO: your app name

target_link_libraries(app
  PRIVATE
    project_options
    project_sanitizers
    project_coverage
)