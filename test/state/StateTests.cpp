#include <catch.hpp>
#include <rapidcheck/catch.h>
#include <rapidcheck/state.h>

#include <typeindex>

#include "util/IntVec.h"
#include "util/GenUtils.h"

using namespace rc;
using namespace rc::detail;
using namespace rc::test;

TEST_CASE("state::isValidCommand") {
  SECTION("returns true for valid commands") {
    REQUIRE(isValidCommand(PushBack(1337), IntVec()));
  }

  SECTION("returns false for invalid commands") {
    REQUIRE(!isValidCommand(PopBack(), IntVec()));
  }
}

namespace {

struct A : public IntVecCmd {};
struct B : public IntVecCmd {};
struct C : public IntVecCmd {};

struct DualConstructible : public IntVecCmd {
  DualConstructible();
  DualConstructible(const IntVec &s)
      : state(s) {}
  IntVec state;
};

struct AlwaysDiscard : public IntVecCmd {
  AlwaysDiscard() { RC_DISCARD("Nope"); }
};

} // namespace

TEST_CASE("state::execOneOf") {
  prop("returns one of the commands",
       [](const GenParams &params, const IntVec &s0) {
         const auto cmd =
             state::gen::execOneOf<A, B, C>(s0)(params.random, params.size)
                 .value();
         const auto &cmdRef = *cmd;
         const auto &id = typeid(cmdRef);
         RC_ASSERT((id == typeid(A)) || (id == typeid(B)) || (id == typeid(C)));
       });

  prop("all commands are eventually returned",
       [](const GenParams &params, const IntVec &s0) {
         auto r = params.random;
         const auto gen = state::gen::execOneOf<A, B, C>(s0);
         std::set<std::type_index> all{typeid(A), typeid(B), typeid(C)};
         std::set<std::type_index> generated;
         while (generated != all) {
           const auto cmd = gen(r.split(), params.size).value();
           const auto &cmdRef = *cmd;
           generated.emplace(typeid(cmdRef));
         }
         RC_SUCCEED("All generated");
       });

  prop("uses state constructor if there is one, passing it the state",
       [](const GenParams &params, const IntVec &s0) {
         const auto cmd = state::gen::execOneOf<DualConstructible>(s0)(
                              params.random, params.size)
                              .value();
         RC_ASSERT(static_cast<const DualConstructible &>(*cmd).state == s0);
       });
}

TEST_CASE("state::check") {
  prop("if no command fails, check succeeds",
       [](const IntVec &s0, IntVec sut) {
         state::check(s0, sut, &state::gen::execOneOf<PushBack>);
       });

  prop("if some command fails, check fails",
       [](const IntVec &s0, IntVec sut) {
         try {
           state::check(s0, sut, &state::gen::execOneOf<AlwaysFail>);
           RC_FAIL("Check succeeded");
         } catch (const CaseResult &result) {
           RC_ASSERT(result.type == CaseResult::Type::Failure);
         }
       });
}
