include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.6.0
)
FetchContent_MakeAvailable(Catch2)

add_library(matchcore_lib
  src/OrderBook.cpp
  src/MatchingEngine.cpp
  src/FeedHandler.cpp
)
target_include_directories(matchcore_lib PUBLIC include)
target_compile_options(matchcore_lib PRIVATE
  -Wall -Wextra
  $<$<CONFIG:Release>:-O3 -march=native>
)

add_executable(matchcore_bench
  bench/replay_bench.cpp
)
target_link_libraries(matchcore_bench PRIVATE matchcore_lib)
target_compile_options(matchcore_bench PRIVATE
  $<$<CONFIG:Release>:-O3 -march=native>
)

add_executable(matchcore_tests
  tests/test_placeholder.cpp
  tests/test_orderbook.cpp
  tests/test_matching_engine.cpp
)
target_link_libraries(matchcore_tests PRIVATE matchcore_lib Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(matchcore_tests DISCOVERY_MODE PRE_TEST)
