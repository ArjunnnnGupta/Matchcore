#include <catch2/catch_test_macros.hpp>

// Phase 0 exit criteria: confirms the CMake + Catch2 toolchain works
// end to end before any real engine code is tested.
TEST_CASE("toolchain sanity check", "[phase0]") {
    REQUIRE(1 + 1 == 2);
}
