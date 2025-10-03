#!/bin/bash
export GTEST_COLOR=yes
cd build
ctest --output-on-failure
exit $ctest_exit_code