#pragma once

#include <string>
#include <vector>

// SF.7: Don’t write using namespace at global scope in a header file
// https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rs-using-directive

std::vector<std::string> SplitIntoWords(const std::string& text);
