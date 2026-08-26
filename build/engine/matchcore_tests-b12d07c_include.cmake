if(EXISTS "/Users/arjungupta/Desktop/Master/Finance/matchcore/build/engine/matchcore_tests")
  if(NOT EXISTS "/Users/arjungupta/Desktop/Master/Finance/matchcore/build/engine/matchcore_tests-b12d07c_tests.cmake" OR
     NOT "/Users/arjungupta/Desktop/Master/Finance/matchcore/build/engine/matchcore_tests-b12d07c_tests.cmake" IS_NEWER_THAN "/Users/arjungupta/Desktop/Master/Finance/matchcore/build/engine/matchcore_tests" OR
     NOT "/Users/arjungupta/Desktop/Master/Finance/matchcore/build/engine/matchcore_tests-b12d07c_tests.cmake" IS_NEWER_THAN "${CMAKE_CURRENT_LIST_FILE}")
    include("/Users/arjungupta/Desktop/Master/Finance/matchcore/build/_deps/catch2-src/extras/CatchAddTests.cmake")
    catch_discover_tests_impl(
      TEST_EXECUTABLE [==[/Users/arjungupta/Desktop/Master/Finance/matchcore/build/engine/matchcore_tests]==]
      TEST_EXECUTOR [==[]==]
      TEST_WORKING_DIR [==[/Users/arjungupta/Desktop/Master/Finance/matchcore/build/engine]==]
      TEST_SPEC [==[]==]
      TEST_EXTRA_ARGS [==[]==]
      TEST_PROPERTIES [==[]==]
      TEST_PREFIX [==[]==]
      TEST_SUFFIX [==[]==]
      TEST_LIST [==[matchcore_tests_TESTS]==]
      TEST_REPORTER [==[]==]
      TEST_OUTPUT_DIR [==[]==]
      TEST_OUTPUT_PREFIX [==[]==]
      TEST_OUTPUT_SUFFIX [==[]==]
      CTEST_FILE [==[/Users/arjungupta/Desktop/Master/Finance/matchcore/build/engine/matchcore_tests-b12d07c_tests.cmake]==]
      TEST_DL_PATHS [==[]==]
      CTEST_FILE [==[]==]
    )
  endif()
  include("/Users/arjungupta/Desktop/Master/Finance/matchcore/build/engine/matchcore_tests-b12d07c_tests.cmake")
else()
  add_test(matchcore_tests_NOT_BUILT matchcore_tests_NOT_BUILT)
endif()
