#include "search_server.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <execution>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "document.h"
#include "string_processing.h"

using namespace std;

SearchServer::SearchServer(const string& stop_words)
    : SearchServer(string_view(stop_words)) {
}

SearchServer::SearchServer(const string_view stop_words)
    : SearchServer(SplitIntoWords(stop_words)) {
}

void SearchServer::AddDocument(int document_id, const string_view document, DocumentStatus status, const vector<int>& ratings) {
    if (document_id < 0) {
        throw invalid_argument("Document's id is out of range"s);
    }
    if (documents_.count(document_id) > 0) {
        throw invalid_argument("Document's id alredy exists"s);
    }
    vector<string> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (string& word : words) {
        // store word in the words storage...
        auto [it, inserted] = words_.insert(move(word));
        // ...end use it's string view
        string_view word_sv = *it;
        word_to_document_freqs_[word_sv][document_id] += inv_word_count;
        document_id_to_word_freqs_[document_id][word_sv] += inv_word_count;
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
    vector<string_view> empty_words;
    for (auto& word_freqs : document_id_to_word_freqs_[document_id]) {
        auto& doc_freqs = word_to_document_freqs_[word_freqs.first];
        doc_freqs.erase(document_id);
        if (doc_freqs.size() == 0)
            empty_words.push_back(word_freqs.first);
    }
    for (const string_view empty_word : empty_words) {
        word_to_document_freqs_.erase(empty_word);
        words_.erase(string(empty_word));
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
    const map<string_view, double>& word_freqs = document_id_to_word_freqs_[document_id];
    // create vector with pointers to words and iterators to erase
    vector<pair<const string_view, map<string_view, map<int, double>>::iterator>> words;
    words.reserve(word_freqs.size());
    const auto keep_it_off = word_to_document_freqs_.end();
    for (const auto& [word, freq] : word_freqs)
        words.emplace_back(make_pair(word, keep_it_off));
    
    // parallel
    for_each(
        execution::par,
        words.begin(),
        words.end(),
        [document_id, this](auto& word_pair) {
            auto iter = word_to_document_freqs_.find(word_pair.first);
            // can erase bacause each thread for unique word
            iter->second.erase(document_id);
            // flag empty word to erase later
            if (iter->second.empty())
                word_pair.second = iter;
        } );

    // erase empty words
    for (const auto [empty_word, erase_iter] : words) {
        if (erase_iter != keep_it_off) {
            word_to_document_freqs_.erase(erase_iter);
            words_.erase(string(empty_word));
        }
    }

    document_id_to_word_freqs_.erase(document_id);
    documents_.erase(document_id);
    document_ids_.erase(document_id);

}

vector<Document>
SearchServer::FindTopDocuments(const string_view raw_query) const
{
    return FindTopDocuments(execution::seq, raw_query);
}

vector<Document>
SearchServer::FindTopDocuments(const string_view raw_query, DocumentStatus status) const
{
    return FindTopDocuments(execution::seq, raw_query, status);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

#if 0

tuple<vector<string_view>, DocumentStatus>
SearchServer::MatchDocument(const string_view raw_query, int document_id) const {

    auto document_data = documents_.find(document_id);
    if (document_data == documents_.end())
        throw std::out_of_range("document_id not found");

    const Query query = ParseQuery(raw_query);

    for (const string_view word : query.minus_words) {
        auto it = word_to_document_freqs_.find(word);
        if (it != word_to_document_freqs_.end() && it->second.count(document_id) > 0) {
            return {vector<string_view>{}, document_data->second.status};
        }
    }

    vector<string_view> matched_words;
    for (const string_view word : query.plus_words) {
        auto it = word_to_document_freqs_.find(word);
        if (it != word_to_document_freqs_.end() && it->second.count(document_id) > 0) {
            // use it->first referencing to words_
            // because 'word' is to be dangling after 'query' destroyed
            matched_words.push_back(it->first);
        }
    }

    return {move(matched_words), document_data->second.status};
}

#else

tuple<vector<string_view>, DocumentStatus>
SearchServer::MatchDocument(const string_view raw_query, int document_id) const {

    auto document_data = documents_.find(document_id);
    if (document_data == documents_.end())
        throw std::out_of_range("document_id not found");

    set<string_view> matched_words;
    const auto raw_query_end = raw_query.end();
    auto i1 = find_if(raw_query.begin(), raw_query_end, [](char c) { return c != ' '; });
    while (i1 != raw_query_end) {
        auto i2 = find_if(i1, raw_query_end, [](char c) { return c == ' '; });
        const string_view word(i1, i2 - i1);
        QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            // gperftool: std::map::find 40.1%
            auto it = word_to_document_freqs_.find(query_word.data);
            // gperftool: std::map::count 47.7%
            if (it != word_to_document_freqs_.end() && it->second.count(document_id) > 0) {
                // document contains query_word
                if (query_word.is_minus) {
                    return {vector<string_view>{}, document_data->second.status};
                } else {
                    matched_words.insert(it->first);
                }
            }
        }
        i1 = find_if(i2, raw_query_end, [](char c) { return c != ' ';});
    }

    vector<string_view> matched_words_vec {matched_words.begin(), matched_words.end()};

    return {move(matched_words_vec), document_data->second.status};
}

#endif

tuple<vector<string_view>, DocumentStatus>
SearchServer::MatchDocument(const execution::sequenced_policy &, const string_view raw_query, int document_id) const {
    return MatchDocument(raw_query, document_id);
}

tuple<vector<string_view>, DocumentStatus>
SearchServer::MatchDocument(const execution::parallel_policy &, const string_view raw_query, int document_id) const {

    auto document_data = documents_.find(document_id);
    if (document_data == documents_.end())
        throw std::out_of_range("document_id not found");

    // gproftools: SplitIntoWordsViews 22.4%
    vector<string_view> query_words = SplitIntoWordsViews(raw_query);

    // process query_words in parallel, replace non-matched words with empty string
    static const string_view empty("");
    bool has_minus_word = false;
    // gproftools: for_each 72.4%
    for_each(
        execution::par,
        query_words.begin(), query_words.end(),
        [this, document_id, &has_minus_word](string_view& word) {
            if (has_minus_word) {
                word = empty;
                return; // TODO: interrupt for_each. how?
            }
            QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                auto it = word_to_document_freqs_.find(query_word.data);
                if (it != word_to_document_freqs_.end() && it->second.count(document_id) > 0) {
                    // document contains query_word
                    if (!query_word.is_minus) {
                        word = it->first;
                        return;
                    } else {
                        has_minus_word = true;
                        word = empty;
                        return;
                    }
                }
            }
            // replace non-matched words with empty string
            word = empty;
        });

    if (has_minus_word)
        return {vector<string_view>{}, document_data->second.status};

    // sort query_words, move empty elements to the back
    // complexity N*log(N) is the same as adding elements to set
    // but don't need dynamic allocations
    sort(query_words.begin(), query_words.end(),
        [](const string_view lhs, const string_view rhs) {
            // move empty words to the end
            if (lhs.size() == 0) {
                return false;
            } else {
                if (rhs.size() == 0)
                    return true;
                else
                    return lhs < rhs;
            };
        });
    auto erase_it = unique(query_words.begin(), query_words.end());
    if (erase_it != query_words.begin() && erase_it[-1] == empty)
        --erase_it;
    query_words.erase(erase_it, query_words.end());

    return {move(query_words), document_data->second.status};
}

std::set<int>::const_iterator
SearchServer::begin() const {
    return document_ids_.begin();
}

std::set<int>::const_iterator
SearchServer::end() const {
    return document_ids_.end();
}

const map<string_view, double>&
SearchServer::GetWordFrequencies(int document_id) const {
    static const map<string_view, double> empty {};
    auto it = document_id_to_word_freqs_.find(document_id);
    if (it != document_id_to_word_freqs_.end()) {
        return it->second;
    } else {
        return empty;
    }
}


bool SearchServer::IsStopWord(const string_view word) const {
    return stop_words_.count(word) > 0;
}

vector<string> SearchServer::SplitIntoWordsNoStop(const string_view text) const {
    vector<string> words;
    for (string& word : SplitIntoWords(text)) {
        if (!IsValidWord(word))
            throw invalid_argument("Invalid character"s);
        if (!IsStopWord(word)) {
            words.push_back(move(word));
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
SearchServer::ParseQueryWord(string_view text) const {
    if (text.empty())
        throw invalid_argument("Empty query word"s);

    bool is_minus = false;

    if (text[0] == '-') {
        if (text.size() < 2)
            throw invalid_argument("Minus-word doesn't contain characters after '-'"s);
        if (text[1] == '-')
            throw invalid_argument("Minus-word starts with '--'"s);
        is_minus = true;
        text.remove_prefix(1);
    }

    if (!IsValidWord(text))
        throw invalid_argument("Query word contains invalid character"s);

    bool is_stop = IsStopWord(text);

    return {
        text,
        is_minus,
        is_stop
    };
}

SearchServer::Query
SearchServer::ParseQuery(const string_view atext) const {
    // copy text and is it for string_view
    Query query {string(atext), {}, {}};

    set<string_view> plus_words_set;
    set<string_view> minus_words_set;
    auto i1 = find_if(query.text.begin(), query.text.end(), [](char c) { return c != ' '; });
    while (i1 != query.text.end()) {
        auto i2 = find_if(i1, query.text.end(), [](char c) { return c == ' ';});
        const string_view word(&(*i1), i2 - i1);
        QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                minus_words_set.insert(query_word.data);
            } else {
                plus_words_set.insert(query_word.data);
            }
        }
        i1 = find_if(i2, query.text.end(), [](char c) { return c != ' ';});
    }

    query.plus_words = vector<string_view>(plus_words_set.begin(), plus_words_set.end());
    query.minus_words = vector<string_view>(minus_words_set.begin(), minus_words_set.end());
    return query;
}

vector<string_view>
SearchServer::SplitIntoWordsViews(const string_view raw_query) const {
    // код с циклами while работает быстрее, чем код с find_if и find (как в SplitIntoWords)
    vector<string_view> query_words;
    query_words.reserve(1000);
    auto i1 = raw_query.begin();
    while (i1 != raw_query.end()) {
        // skip spaces
        while (i1 != raw_query.end() && *i1 == ' ')
            ++i1;
        if (i1 == raw_query.end())
            break;
        // find end of word
        auto i2 = i1;
        while (i2 != raw_query.end() && *i2 != ' ')
            ++i2;
        query_words.emplace_back(i1, i2 - i1);
        i1 = i2;
    }
    return query_words; // NRVO
}

double SearchServer::ComputeWordInverseDocumentFreq(const string_view word) const {
    const auto& it = word_to_document_freqs_.find(word);
    assert(it != word_to_document_freqs_.end());
    return ComputeWordInverseDocumentFreq(it->second.size());
}

double SearchServer::ComputeWordInverseDocumentFreq(int docs_with_word) const {
    assert(docs_with_word > 0);
    return log(static_cast<double>(GetDocumentCount())
               / static_cast<double>(docs_with_word));
}

bool SearchServer::IsValidWord(const string_view word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}
