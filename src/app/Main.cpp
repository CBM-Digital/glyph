#include "script/Error.h"
#include "script/VM.h"

#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char** argv) {
  std::ostringstream source;
  std::string file = "<stdin>";

  if (argc > 1) {
    file = argv[1];
    std::ifstream input(file);
    if (!input) {
      std::cerr << "unable to open " << file << '\n';
      return 1;
    }
    source << input.rdbuf();
  } else {
    source << std::cin.rdbuf();
  }

  try {
    glyph::script::VM vm;
    const auto value = vm.evalSource(source.str(), file);
    std::cout << glyph::script::valueToString(value, vm.interner()) << '\n';
  } catch (const glyph::script::ScriptError& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }

  return 0;
}
