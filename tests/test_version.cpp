#include <catch2/catch_test_macros.hpp>

#include "core/version.hpp"

TEST_CASE("hello world: version is a non-empty semver-ish string", "[core]") {
  const auto v = gsr::core::version();
  REQUIRE_FALSE(v.empty());
  REQUIRE(v.find('.') != std::string_view::npos);
}

TEST_CASE("cuda_enabled reports a stable answer", "[core]") {
  REQUIRE(gsr::core::cuda_enabled() == gsr::core::cuda_enabled());
}
