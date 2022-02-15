#include "search_server.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <string>

#include "document.h"
#include "string_processing.h"

using namespace std;

SearchServer::SearchServer(const string& stop_words)
    : SearchServer(SplitIntoWords(stop_words)) {
}

void SearchServer::AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
    if (document_id < 0)
        throw invalid_argument("Document's id is out of range"s);
    if (documents_.count(document_id) > 0)
        throw invalid_argument("Document's id alredy exists"s);
    const vector<string> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (const string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
    }
    documents_.emplace(document_id, 
        DocumentData{
            ComputeAverageRating(ratings), 
            status
        });
    document_ids_.push_back(document_id);
}

vector<Document>
SearchServer::FindTopDocuments(const string& raw_query) const
{
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

vector<Document>
SearchServer::FindTopDocuments(const string& raw_query, DocumentStatus status) const
{
    return FindTopDocuments(raw_query,
        [status](int id, DocumentStatus st, int rating) {
            (void)id;
            (void)rating;
            return st == status;} );
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

tuple<vector<string>, DocumentStatus>
SearchServer::MatchDocument(const string& raw_query, int document_id) const {
    const Query query = ParseQuery(raw_query);
    vector<string> matched_words;
    for (const string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    return {matched_words, documents_.at(document_id).status};
}

int SearchServer::GetDocumentId(int index) const {
    // method 'at' throws exception if index is out of range
    return static_cast<int>(document_ids_.at(index));
}

void SearchServer::SetStopWords(const string& text) {
    for (const string& word : SplitIntoWords(text)) {
        stop_words_.insert(word);
    }
}    

bool SearchServer::IsStopWord(const string& word) const {
    return stop_words_.count(word) > 0;
}

vector<string> SearchServer::SplitIntoWordsNoStop(const string& text) const {
    vector<string> words;
    for (const string& word : SplitIntoWords(text)) {
        if (!IsValidWord(word))
            throw invalid_argument("Invalid character"s);
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord
SearchServer::ParseQueryWord(string text) const {
    if (text.empty())
        throw invalid_argument("Empty query word"s);

    bool is_minus = false;

    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
        if (text.empty())
            throw invalid_argument("Minus-word doesn't contain characters after '-'"s);
        if (text[0] == '-')
            throw invalid_argument("Minus-word starts with '--'"s);
    }

    if (!IsValidWord(text))
        throw invalid_argument("Query word contains invalid character"s);

    return {
        text,
        is_minus,
        IsStopWord(text)
    };
}

SearchServer::Query
SearchServer::ParseQuery(const string& text) const {
    Query query;
    for (const string& word : SplitIntoWords(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.insert(query_word.data);
            } else {
                query.plus_words.insert(query_word.data);
            }
        }
    }
    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(const string& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

bool SearchServer::IsValidWord(const string& word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}
