#include "script/Error.h"
#include "script/Lexer.h"
#include "script/Parser.h"
#include "script/VM.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
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

} // namespace

int main() {
  try {
    testLexer();
    testParser();
    testInterpreterAcceptance();
    testSpecialForms();
    testClosuresForAndKeywordAccess();
  } catch (const glyph::script::ScriptError& error) {
    std::cerr << "ScriptError: " << error.what() << '\n';
    return 1;
  }

  std::cout << "glyph script core tests passed\n";
  return 0;
}
