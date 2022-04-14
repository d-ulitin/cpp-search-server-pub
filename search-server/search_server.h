#pragma once

#include <algorithm>
#include <execution>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <string_view>
#include <cassert>

// SF.7: Don’t write using namespace at global scope in a header file
// https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rs-using-directive

#include "document.h"
#include "concurrent_map.h"

static inline const double RELEVANCE_EPS = 1e-6;
static inline const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {

public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    
    explicit SearchServer(const std::string& stop_words);
    explicit SearchServer(const std::string_view stop_words);

    SearchServer() = default;

    void AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename Filter, typename ExecutionPolicy>
    std::vector<Document>
    FindTopDocuments(ExecutionPolicy&& policy,
                     const std::string_view raw_query, Filter filter) const;

    template <typename Filter>
    std::vector<Document>
    FindTopDocuments(const std::string_view raw_query, Filter filter) const;

    // overload FindTopDocuments with no parameters (return actual documents)
    template<typename ExecutionPolicy>
    std::vector<Document>
    FindTopDocuments(ExecutionPolicy&& policy,
                     const std::string_view raw_query) const;

    std::vector<Document>
    FindTopDocuments(const std::string_view raw_query) const;

    // overload FindTopDocuments with status parameter only
    template<typename ExecutionPolicy>
    std::vector<Document>
    FindTopDocuments(ExecutionPolicy&& policy,
                     const std::string_view raw_query, DocumentStatus status) const;

    std::vector<Document>
    FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const;

    int GetDocumentCount() const;
    
    std::tuple<std::vector<std::string_view>, DocumentStatus>
    MatchDocument(const std::string_view raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus>
    MatchDocument(const std::execution::sequenced_policy& policy, const std::string_view raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus>
    MatchDocument(const std::execution::parallel_policy& policy, const std::string_view raw_query, int document_id) const;

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
    std::vector<std::string> SplitIntoWordsNoStop(const std::string_view text) const;
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
    
    Query ParseQuery(const std::string_view text) const;

    std::vector<std::string_view> SplitIntoWordsViews(const std::string_view text) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string_view word) const;
    double ComputeWordInverseDocumentFreq(int docs_with_word) const;

    template <typename Filter>
    std::vector<Document>
    FindAllDocuments(const std::execution::sequenced_policy&,
                     const Query& query, Filter filter) const;

    template <typename Filter>
    std::vector<Document>
    FindAllDocuments(const std::execution::parallel_policy&,
                     const Query& query, Filter filter) const;

    static bool IsValidWord(const std::string_view word);
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words) {
    using namespace std::literals;
    for (const std::string_view word : stop_words) {
        if (!IsValidWord(word))
            throw std::invalid_argument("Stop-word contains invalid character"s);
        if (!word.empty()) {
            auto [it, inserted] = words_.insert(std::string(word));
            assert(inserted);
            auto [it_view, inserted_view] = stop_words_.insert(*it);
            assert(inserted_view);
        }
    }
}

template <typename ExecutionPolicy>
std::vector<Document>
SearchServer::FindTopDocuments(ExecutionPolicy&& policy,
                               const std::string_view raw_query) const
{
    return FindTopDocuments(std::forward<ExecutionPolicy>(policy), raw_query,
                            DocumentStatus::ACTUAL);
}

template <typename ExecutionPolicy>
std::vector<Document>
SearchServer::FindTopDocuments(ExecutionPolicy&& policy,
                               const std::string_view raw_query, DocumentStatus status) const
{
    return FindTopDocuments(std::forward<ExecutionPolicy>(policy), raw_query,
        [status](int id, DocumentStatus st, int rating) {
            (void)id;
            (void)rating;
            return st == status;} );
}


template <typename Filter, typename ExecutionPolicy>
std::vector<Document>
SearchServer::FindTopDocuments(ExecutionPolicy&& policy,
                               const std::string_view raw_query, Filter filter) const {            
    const Query query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(policy, query, filter);
    
    // cumulative time of sort is about 5%, don't need to be parallel
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
SearchServer::FindTopDocuments(const std::string_view raw_query, Filter filter) const {
    
    return FindTopDocuments(std::execution::seq, raw_query, filter);
}


template <typename Filter>
std::vector<Document>
SearchServer::FindAllDocuments(const std::execution::sequenced_policy&,
                               const Query& query, Filter filter) const {
    std::map<int, double> document_to_relevance;
    for (const std::string_view word : query.plus_words) {
        const auto doc_freqs_it = word_to_document_freqs_.find(word);
        if (doc_freqs_it == word_to_document_freqs_.end()) {
            continue;
        }
        const double inverse_document_freq =
            ComputeWordInverseDocumentFreq(doc_freqs_it->second.size());
        for (const auto [document_id, term_freq] : doc_freqs_it->second) {
            const auto document_it = documents_.find(document_id);
            assert(document_it != documents_.end());
            if (filter(document_id, document_it->second.status, document_it->second.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }
    
    for (const std::string_view word : query.minus_words) {
        const auto doc_freqs_it = word_to_document_freqs_.find(word);
        if (doc_freqs_it == word_to_document_freqs_.end()) {
            continue;
        }
        for (const auto [document_id, term_freq] : doc_freqs_it->second) {
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

template <typename Filter>
std::vector<Document>
SearchServer::FindAllDocuments(const std::execution::parallel_policy&,
                               const Query& query, Filter filter) const {
    const int bucket_number = 128;
    // Tests (-O2):
    // bucket_number    time      %
    //             8   11392   
    //            16    8251   -27%
    //            32    6705   -18%
    //            64    5908   -12%
    //           128    5386    -8%
    //           256    5183    -4%
    ConcurrentMap<int, double> document_to_relevance(bucket_number);
    std::for_each(
        std::execution::par,
        query.plus_words.begin(),
        query.plus_words.end(),
        [this, &document_to_relevance, &filter](const std::string_view word) {
            const auto doc_freqs_it = word_to_document_freqs_.find(word);
            if (doc_freqs_it == word_to_document_freqs_.end()) {
                return;
            }
            const double inverse_document_freq =
                ComputeWordInverseDocumentFreq(doc_freqs_it->second.size());
            
            for (const auto [document_id, term_freq] : doc_freqs_it->second) { // this line 9% of total time (operator++ of map tree)
                const auto document_it = documents_.find(document_id); // this line 16% of total run time
                assert(document_it != documents_.end());
                if (filter(document_id, document_it->second.status, document_it->second.rating)) {
                    auto access = document_to_relevance[document_id]; // this line 28% of total time (~14% mutex lock/unlock, ~14% map::operator[])
                    access.ref_to_value += term_freq * inverse_document_freq;
                }
            }
        }
    );
    
    std::for_each( // fast, no need to parallel
        query.minus_words.begin(),
        query.minus_words.end(),
        [this, &document_to_relevance](const std::string_view word) {
            const auto doc_freqs_it = word_to_document_freqs_.find(word);
            if (doc_freqs_it == word_to_document_freqs_.end()) {
                return;
            }
            for (const auto [document_id, term_freq] : doc_freqs_it->second) {
                auto n = document_to_relevance.erase(document_id);
                assert(n == 1);
            }
        }
    );


    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) { // BuildOrdinaryMap - 10%
        matched_documents.push_back({
            document_id,
            relevance,
            documents_.at(document_id).rating
        });
    }
    return matched_documents;
}