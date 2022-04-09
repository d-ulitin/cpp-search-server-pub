#pragma once

#include <algorithm>
#include <execution>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include <string_view>
#include <cassert>

// SF.7: Don’t write using namespace at global scope in a header file
// https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rs-using-directive

#include "document.h"

static inline const double RELEVANCE_EPS = 1e-6;
static inline const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {

public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    
    explicit SearchServer(const std::string& stop_words);

    SearchServer() = default;

    void AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename Filter>
    std::vector<Document> FindTopDocuments(const std::string& raw_query, Filter filter) const;

    // overload FindTopDocuments with no parameters (return actual documents)
    std::vector<Document> FindTopDocuments(const std::string& raw_query) const;

    // overload FindTopDocuments with status parameter only
    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentStatus status) const;

    int GetDocumentCount() const;
    
    std::tuple<std::vector<std::string>, DocumentStatus>
    MatchDocument(const std::string& raw_query, int document_id) const;

    std::tuple<std::vector<std::string>, DocumentStatus>
    MatchDocument(const std::execution::sequenced_policy& policy, const std::string& raw_query, int document_id) const;

    std::tuple<std::vector<std::string>, DocumentStatus>
    MatchDocument(const std::execution::parallel_policy& policy, const std::string& raw_query, int document_id) const;

    std::set<int>::const_iterator begin() const;

    std::set<int>::const_iterator end() const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

    template <typename ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy ep, int document_id);

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    // words storage; store here all the words of the server
    std::set<std::string> words_;
    // use string_view objects that points to strings from words_ above
    std::set<std::string_view> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> document_id_to_word_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;

    bool IsStopWord(const std::string_view word) const;
    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;
    static int ComputeAverageRating(const std::vector<int>& ratings);
    
    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };
    
    QueryWord ParseQueryWord(std::string_view text) const;
    
    struct Query {
        const std::string text;
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };
    
    Query ParseQuery(const std::string& text) const;
    Query ParseQuery(const std::execution::sequenced_policy& policy, const std::string& text) const;
    Query ParseQuery(const std::execution::parallel_policy& policy, const std::string& text) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string_view word) const;

    template <typename Filter>
    std::vector<Document> FindAllDocuments(const Query& query, Filter filter) const;

    static bool IsValidWord(const std::string_view word);
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words) {
    using namespace std::literals;
    for (const std::string& word : stop_words) {
        if (!IsValidWord(word))
            throw std::invalid_argument("Stop-word contains invalid character"s);
        if (!word.empty()) {
            auto [it, inserted] = words_.insert(word);
            assert(inserted);
            auto [it_view, inserted_view] = stop_words_.insert(*it);
            assert(inserted_view);
        }
    }
}

template <typename Filter>
std::vector<Document>
SearchServer::FindTopDocuments(const std::string& raw_query, Filter filter) const {            
    const Query query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(query, filter);
    
    std::sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                // always use std:abs
            if (std::abs(lhs.relevance - rhs.relevance) < RELEVANCE_EPS) {
                return lhs.rating > rhs.rating;
            } else {
                return lhs.relevance > rhs.relevance;
            }
            });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        // without second parameter class Document must have default constructor
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT, Document{0,0,0});
    }
    return matched_documents;
}

template <typename Filter>
std::vector<Document>
SearchServer::FindAllDocuments(const Query& query, Filter filter) const {
    std::map<int, double> document_to_relevance;
    for (const std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document = documents_.at(document_id);
            if (filter(document_id, document.status, document.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }
    
    for (const std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({
            document_id,
            relevance,
            documents_.at(document_id).rating
        });
    }
    return matched_documents;
}