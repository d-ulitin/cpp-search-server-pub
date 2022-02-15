#include "document.h"

#include <string>

using namespace std;

Document::Document(int id, double relevance, int rating)
    : id(id)
    , relevance(relevance)
    , rating(rating) {
}

