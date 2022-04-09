#include "string_processing.h"

#include <algorithm>
#include <utility>

using namespace std;

vector<string> SplitIntoWords(const string& text) {
    if (text.empty())
        return {};
    vector<string> words;

    auto i1 = find_if(text.begin(), text.end(), [](char c) {return c != ' ';});
    while (i1 != text.end()) {
        auto i2 = find(i1, text.end(), ' ');
        words.emplace_back(i1, i2);
        i1 = find_if(i2, text.end(), [](char c) {return c != ' ';});
    }
    return words;
}
