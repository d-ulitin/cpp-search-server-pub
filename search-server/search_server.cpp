#include "search_server.h"

#include <algorithm>
#include <cmath>
#include <execution>
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
    if (document_id < 0) {
        throw invalid_argument("Document's id is out of range"s);
    }
    if (documents_.count(document_id) > 0) {
        throw invalid_argument("Document's id alredy exists"s);
    }
    const vector<string> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (const string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        document_id_to_word_freqs_[document_id][word] += inv_word_count;
    }
    documents_.emplace(document_id, 
        DocumentData{
            ComputeAverageRating(ratings), 
            status
        });
    document_ids_.insert(document_id);
}

void
SearchServer::RemoveDocument(int document_id) {
    if (document_ids_.count(document_id) == 0)
        return;
    set<string> empty_words;
    for (auto& word_freqs : document_id_to_word_freqs_[document_id]) {
        auto& doc_freqs = word_to_document_freqs_[word_freqs.first];
        doc_freqs.erase(document_id);
        if (doc_freqs.size() == 0)
            empty_words.insert(word_freqs.first);
    }
    for (const string& empty_word : empty_words) {
        word_to_document_freqs_.erase(empty_word);
    }
    document_id_to_word_freqs_.erase(document_id);
    documents_.erase(document_id);
    document_ids_.erase(document_id);
}

template <>
void
SearchServer::RemoveDocument(std::execution::sequenced_policy, int document_id) {
    RemoveDocument(document_id);
}

template <>
void
SearchServer::RemoveDocument(std::execution::parallel_policy, int document_id) {
    if (document_ids_.count(document_id) == 0)
        return;

    // get words of the document
    const map<string, double>& word_freqs = document_id_to_word_freqs_[document_id];
    // create vector with pointers to words and iterators to erase
    vector<pair<const string*, map<string, map<int, double>>::iterator>> words;
    words.reserve(word_freqs.size());
    const auto keep_it_off = word_to_document_freqs_.end();
    for (const auto& [word, freq] : word_freqs)
        words.emplace_back(make_pair(&word, keep_it_off));
    
    // parallel
    for_each(
        execution::par,
        words.begin(),
        words.end(),
        [document_id, this](auto& word_pair) {
            auto iter = word_to_document_freqs_.find(*word_pair.first);
            // can erase bacause each thread for unique word
            iter->second.erase(document_id);
            // flag empty word to erase later
            if (iter->second.empty())
                word_pair.second = iter;
        } );

    // erase empty words
    for (const auto& [word, erase_iter] : words) {
        if (erase_iter != keep_it_off)
            word_to_document_freqs_.erase(erase_iter);
    }

    document_id_to_word_freqs_.erase(document_id);
    documents_.erase(document_id);
    document_ids_.erase(document_id);

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

std::set<int>::const_iterator
SearchServer::begin() const {
    return document_ids_.begin();
}

std::set<int>::const_iterator
SearchServer::end() const {
    return document_ids_.end();
}

const map<string, double>&
SearchServer::GetWordFrequencies(int document_id) const {
    static const map<string, double> empty {};
    auto it = document_id_to_word_freqs_.find(document_id);
    if (it != document_id_to_word_freqs_.end()) {
        return it->second;
    } else {
        return empty;
    }
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
