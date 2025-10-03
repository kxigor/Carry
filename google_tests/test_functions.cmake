find_package(GTest REQUIRED)

include_directories(${GTEST_INCLUDE_DIRS})

# .sh - simple test, .cpp - google test

function(add_google_test TEST_NAME TEST_SRC)
    add_executable(${TEST_NAME} ${TEST_SRC})
    target_include_directories(${TEST_NAME} SYSTEM PUBLIC
        Threads::Threads
        ${GTEST_INCLUDE_DIRS}
        ${GMOCK_INCLUDE_DIRS}
    )
    set_property(TARGET ${TEST_NAME} PROPERTY RUNTIME_OUTPUT_DIRECTORY "")
    target_link_libraries(${TEST_NAME} PUBLIC ${GTEST_LIBRARIES})
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endfunction()

function(add_simple_test TEST_NAME TEST_SRC)
    add_executable(${TEST_NAME} ${TEST_SRC})
    set_property(TARGET ${TEST_NAME} PROPERTY RUNTIME_OUTPUT_DIRECTORY "")
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endfunction()

function(copy_dlls_target target)
    if(WIN32)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:${target}>
            $<TARGET_FILE_DIR:${target}>
            COMMAND_EXPAND_LISTS
        )
    endif()
endfunction()