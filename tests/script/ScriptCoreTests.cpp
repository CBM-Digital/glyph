#include "script/Error.h"
#include "script/Lexer.h"
#include "script/Parser.h"
#include "script/VM.h"

#include <cstdlib>
#include <cmath>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

void requireNumber(const glyph::script::Value& value, double expected, const std::string& message) {
  require(value.kind == glyph::script::ValueKind::Number, message + " kind");
  require(std::fabs(value.number - expected) < 0.000001, message);
}

void testLexer() {
  glyph::script::Lexer lexer("(def answer 42) ; ignored\n[:x \"ok\"]");
  const auto tokens = lexer.lex();
  require(tokens.size() == 10, "lexer token count");
  require(tokens[0].type == glyph::script::TokenType::LeftParen, "lexer left paren");
  require(tokens[2].text == "answer", "lexer symbol text");
  require(tokens[3].number == 42.0, "lexer number");
  require(tokens[6].text == ":x", "lexer keyword");
  require(tokens[7].text == "ok", "lexer string");
}

void testParser() {
  glyph::StringInterner interner;
  glyph::script::Lexer lexer("{:x (+ 1 2) :name \"player\"}");
  const auto tokens = lexer.lex();
  glyph::script::Parser parser(tokens, interner);
  const auto program = parser.parseProgram();
  require(program.size() == 1, "parser program size");
  require(program[0]->kind == glyph::script::AstKind::Map, "parser map");
  require(program[0]->entries.size() == 2, "parser map entries");
}

void testInterpreterAcceptance() {
  glyph::script::VM vm;
  const auto value = vm.evalSource("(defn add [a b] (+ a b))\n(add 1 2)");
  require(value.kind == glyph::script::ValueKind::Number, "acceptance result kind");
  require(value.number == 3.0, "acceptance returns 3");
}

void testSpecialForms() {
  glyph::script::VM vm;
  const auto value = vm.evalSource(R"(
    (defn classify [x]
      (let [limit 10
            y (+ x 1)]
        (cond
          (< y limit) :small
          (= y limit) :edge
          else :large)))
    [(classify 1)
     (classify 9)
     (classify 12)]
  )");

  require(value.kind == glyph::script::ValueKind::Vector, "special forms vector result");
  require(value.vector->size() == 3, "special forms vector size");
  require(glyph::script::valueToString((*value.vector)[0], vm.interner()) == ":small",
          "cond first branch");
  require(glyph::script::valueToString((*value.vector)[1], vm.interner()) == ":edge",
          "cond second branch");
  require(glyph::script::valueToString((*value.vector)[2], vm.interner()) == ":large",
          "cond else branch");
}

void testClosuresForAndKeywordAccess() {
  glyph::script::VM vm;
  const auto value = vm.evalSource(R"(
    (def make-adder (fn [n] (fn [x] (+ n x))))
    (def add-five (make-adder 5))
    (def state {:items [{:x 1} {:x 2} {:x 3}]})
    (for [item (:items state)]
      (add-five (:x item)))
  )");

  require(value.kind == glyph::script::ValueKind::Vector, "for returns vector");
  require(value.vector->size() == 3, "for mapped count");
  require((*value.vector)[0].number == 6.0, "closure first result");
  require((*value.vector)[2].number == 8.0, "closure last result");
}

void testStdLibAcceptance() {
  glyph::script::VM vm;
  const auto value = vm.evalSource("(update-in {:player {:x 10}} [:player :x] + 5)");
  require(value.kind == glyph::script::ValueKind::Map, "update-in returns map");
  const auto player = value.map->at(vm.interner().intern(":player"));
  require(player.kind == glyph::script::ValueKind::Map, "update-in nested map");
  requireNumber(player.map->at(vm.interner().intern(":x")), 15.0, "update-in acceptance returns 15");
}

void testMathStringAndBooleanStdLib() {
  glyph::script::VM vm;
  const auto value = vm.evalSource(R"(
    [(mod 7 4)
     (abs -3)
     (floor 3.8)
     (ceil 3.2)
     (round 3.5)
     (min 4 2 8)
     (max 4 2 8)
     (clamp 12 0 10)
     (lerp 10 20 0.25)
     (round (rad->deg (deg->rad 90)))
     (dist 0 0 3 4)
     (str "Score " 12 :pts)
     (substr "abcdef" 2 3)
     (string-length "abcdef")
     (and true false missing-symbol)
     (or true missing-symbol)]
  )");

  require(value.kind == glyph::script::ValueKind::Vector, "math test vector");
  requireNumber((*value.vector)[0], 3.0, "mod");
  requireNumber((*value.vector)[1], 3.0, "abs");
  requireNumber((*value.vector)[2], 3.0, "floor");
  requireNumber((*value.vector)[3], 4.0, "ceil");
  requireNumber((*value.vector)[4], 4.0, "round");
  requireNumber((*value.vector)[5], 2.0, "min");
  requireNumber((*value.vector)[6], 8.0, "max");
  requireNumber((*value.vector)[7], 10.0, "clamp");
  requireNumber((*value.vector)[8], 12.5, "lerp");
  requireNumber((*value.vector)[9], 90.0, "deg rad conversion");
  requireNumber((*value.vector)[10], 5.0, "dist");
  require((*value.vector)[11].text == "Score 12:pts", "str");
  require((*value.vector)[12].text == "cde", "substr");
  requireNumber((*value.vector)[13], 6.0, "string-length");
  require((*value.vector)[14].kind == glyph::script::ValueKind::Bool && !(*value.vector)[14].boolean,
          "and short circuits");
  require((*value.vector)[15].kind == glyph::script::ValueKind::Bool && (*value.vector)[15].boolean,
          "or short circuits");
}

void testMapStdLib() {
  glyph::script::VM vm;
  const auto value = vm.evalSource(R"(
    (def original {:x 1 :player {:hp 3}})
    (def changed (assoc original :x 2 :y 4))
    [(get original :x)
     (get original :missing 99)
     (get changed :x)
     (get changed :y)
     (get-in original [:player :hp])
     (get-in original [:player :missing] 7)
     (get-in (assoc-in original [:player :hp] 10) [:player :hp])
     (get-in (update original :x + 9) [:x])
     (get-in (merge {:a 1 :b 2} {:b 3}) [:b])
     (has? original :player)
     (count (keys original))
     (count (values original))]
  )");

  require(value.kind == glyph::script::ValueKind::Vector, "map stdlib vector");
  requireNumber((*value.vector)[0], 1.0, "assoc leaves original");
  requireNumber((*value.vector)[1], 99.0, "get fallback");
  requireNumber((*value.vector)[2], 2.0, "assoc updated key");
  requireNumber((*value.vector)[3], 4.0, "assoc added key");
  requireNumber((*value.vector)[4], 3.0, "get-in");
  requireNumber((*value.vector)[5], 7.0, "get-in fallback");
  requireNumber((*value.vector)[6], 10.0, "assoc-in");
  requireNumber((*value.vector)[7], 10.0, "update");
  requireNumber((*value.vector)[8], 3.0, "merge override");
  require((*value.vector)[9].boolean, "has?");
  requireNumber((*value.vector)[10], 2.0, "keys count");
  requireNumber((*value.vector)[11], 2.0, "values count");
}

void testVectorStdLib() {
  glyph::script::VM vm;
  const auto value = vm.evalSource(R"(
    [(count [1 2 3])
     (empty? [])
     (first [9 8])
     (last [9 8])
     (nth [9 8] 1)
     (nth [9 8] 7 42)
     (conj [1] 2 3)
     (pop [1 2 3])
     (slice [1 2 3 4] 1 3)
     (map [1 2 3] (fn [x] (* x 2)))
     (filter [1 2 3 4] (fn [x] (> x 2)))
     (remove [1 2 3 4] (fn [x] (> x 2)))
     (any [1 2 3] (fn [x] (= x 2)))
     (all [1 2 3] (fn [x] (< x 4)))
     (find [1 2 3] (fn [x] (> x 1)))
     (range 2 5)]
  )");

  require(value.kind == glyph::script::ValueKind::Vector, "vector stdlib vector");
  requireNumber((*value.vector)[0], 3.0, "count vector");
  require((*value.vector)[1].boolean, "empty?");
  requireNumber((*value.vector)[2], 9.0, "first");
  requireNumber((*value.vector)[3], 8.0, "last");
  requireNumber((*value.vector)[4], 8.0, "nth");
  requireNumber((*value.vector)[5], 42.0, "nth fallback");
  require(glyph::script::valueToString((*value.vector)[6], vm.interner()) == "[1 2 3]", "conj");
  require(glyph::script::valueToString((*value.vector)[7], vm.interner()) == "[1 2]", "pop");
  require(glyph::script::valueToString((*value.vector)[8], vm.interner()) == "[2 3]", "slice");
  require(glyph::script::valueToString((*value.vector)[9], vm.interner()) == "[2 4 6]", "map");
  require(glyph::script::valueToString((*value.vector)[10], vm.interner()) == "[3 4]", "filter");
  require(glyph::script::valueToString((*value.vector)[11], vm.interner()) == "[1 2]", "remove");
  require((*value.vector)[12].boolean, "any");
  require((*value.vector)[13].boolean, "all");
  requireNumber((*value.vector)[14], 2.0, "find");
  require(glyph::script::valueToString((*value.vector)[15], vm.interner()) == "[2 3 4]", "range");
}

void testRandomAndCollisionStdLib() {
  glyph::script::VM vm;
  const auto value = vm.evalSource(R"(
    (def r (rand))
    (def ri (rand-int 3))
    (def rr (rand-range 2 4))
    (def picked (choose [:a :b :c]))
    [(and (>= r 0) (< r 1))
     (and (>= ri 0) (< ri 3))
     (and (>= rr 2) (<= rr 4))
     (not= picked nil)
     (chance 1)
     (overlap? (rect-box 0 0 10 10) (rect-box 5 5 10 10))
     (point-in-rect? 2 3 (rect-box 0 0 10 10))
     (:x (rect-center (rect-box 0 0 10 20)))
     (circle-overlap? (circle-box 0 0 5) (circle-box 8 0 5))
     (point-in-circle? 3 4 (circle-box 0 0 5))]
  )");

  require(value.kind == glyph::script::ValueKind::Vector, "random collision vector");
  for (std::size_t i = 0; i < 7; ++i) {
    require((*value.vector)[i].kind == glyph::script::ValueKind::Bool && (*value.vector)[i].boolean,
            "random/collision boolean " + std::to_string(i));
  }
  requireNumber((*value.vector)[7], 5.0, "rect-center x");
  require((*value.vector)[8].boolean, "circle-overlap?");
  require((*value.vector)[9].boolean, "point-in-circle?");
}

} // namespace

int main() {
  try {
    testLexer();
    testParser();
    testInterpreterAcceptance();
    testSpecialForms();
    testClosuresForAndKeywordAccess();
    testStdLibAcceptance();
    testMathStringAndBooleanStdLib();
    testMapStdLib();
    testVectorStdLib();
    testRandomAndCollisionStdLib();
  } catch (const glyph::script::ScriptError& error) {
    std::cerr << "ScriptError: " << error.what() << '\n';
    return 1;
  }

  std::cout << "glyph script core tests passed\n";
  return 0;
}
