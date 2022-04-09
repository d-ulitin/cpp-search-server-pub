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
    : SearchServer(SplitIntoWords(stop_words)) {
}

void SearchServer::AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
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

    auto document_data = documents_.find(document_id);
    assert(document_data != documents_.end());

    const Query query = ParseQuery(raw_query);

    for (const string_view word : query.minus_words) {
        auto it = word_to_document_freqs_.find(word);
        if (it != word_to_document_freqs_.end() && it->second.count(document_id) > 0) {
            return {vector<string>{}, document_data->second.status};
        }
    }

    vector<string> matched_words;
    for (const string_view word : query.plus_words) {
        auto it = word_to_document_freqs_.find(word);
        if (it != word_to_document_freqs_.end() && it->second.count(document_id) > 0) {
            matched_words.emplace_back(word);
        }
    }

    return {move(matched_words), document_data->second.status};
}

tuple<vector<string>, DocumentStatus>
SearchServer::MatchDocument(const execution::sequenced_policy &, const string& raw_query, int document_id) const {
    return MatchDocument(raw_query, document_id);
}

tuple<vector<string>, DocumentStatus>
SearchServer::MatchDocument(const execution::parallel_policy &, const string& raw_query, int document_id) const {

    auto document_data = documents_.find(document_id);
    assert(document_data != documents_.end());

    // профилирование и тесты показали, что
    // обработка запроса - самая долгая операция...
    Query query = ParseQuery(std::execution::par, raw_query);

    // ...а параллельный вариант обработки слов не дает прироста производительности

#if 0

    bool has_minus_word = any_of(
        std::execution::seq,
        query.minus_words.begin(),
        query.minus_words.end(),
        [this, document_id](const string_view word) -> bool {
            auto it = this->word_to_document_freqs_.find(word);
            if (it != this->word_to_document_freqs_.end() && it->second.count(document_id) > 0)
                return true;
            return false;
        });

    if (has_minus_word)
        return {vector<string>{}, document_data->second.status};

    static const string_view empty("");
    for_each(
        std::execution::seq,
        query.plus_words.begin(),
        query.plus_words.end(),
        [this, document_id](string_view& word) {
            auto it = this->word_to_document_freqs_.find(word);
            if (it != this->word_to_document_freqs_.end() && it->second.count(document_id) == 0)
                word = empty;
        }
    );

    vector<string> matched_words;
    for(const string_view word : query.plus_words)
        if (word.size() > 0)
            matched_words.emplace_back(word);

#else

    for (const string_view word : query.minus_words) {
        auto it = word_to_document_freqs_.find(word);
        if (it != word_to_document_freqs_.end() && it->second.count(document_id) > 0) {
            return {vector<string>{}, document_data->second.status};
        }
    }

    vector<string> matched_words;
    for (const string_view word : query.plus_words) {
        auto it = word_to_document_freqs_.find(word);
        if (it != word_to_document_freqs_.end() && it->second.count(document_id) > 0) {
            matched_words.emplace_back(word);
        }
    }

#endif

    return {move(matched_words), document_data->second.status};
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

vector<string> SearchServer::SplitIntoWordsNoStop(const string& text) const {
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
SearchServer::ParseQuery(const string& atext) const {
    // copy text and is it for string_view
    Query query {atext, {}, {}};

    set<string_view> plus_words_set;
    set<string_view> minus_words_set;
    auto i1 = find_if(query.text.begin(), query.text.end(),
        [](char c) { return c != ' '; });
    while (i1 != query.text.end()) {
        auto i2 = find_if(i1, query.text.end(),
            [](char c) { return c == ' ';});

        string_view word(&(*i1), i2 - i1);
        QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                minus_words_set.insert(query_word.data);
            } else {
                plus_words_set.insert(query_word.data);
            }
        }

        i1 = find_if(i2, query.text.end(),
            [](char c) { return c != ' ';});
    }

    query.plus_words = vector<string_view>(plus_words_set.begin(), plus_words_set.end());
    query.minus_words = vector<string_view>(minus_words_set.begin(), minus_words_set.end());
    return query;
}

SearchServer::Query
SearchServer::ParseQuery(const std::execution::sequenced_policy&, const string& atext) const {
    return ParseQuery(atext);
}

SearchServer::Query
SearchServer::ParseQuery(const std::execution::parallel_policy&, const string& atext) const {
    // copy text and is it for string_view
    Query query {atext, {}, {}};

    // Inserting word into set<string_view> is slow and can't be paralleled.
    // To get rid of inserting into a set use this algorithm:
    // 1. Create vectors of plus- and munus-words.
    // 2. Sort them in parallel.
    // 3. Make elements unique.

    vector<string_view> plus_words;
    vector<string_view> minus_words;
    auto i1 = find_if(query.text.begin(), query.text.end(), [](char c) { return c != ' '; });
    while (i1 != query.text.end()) {
        auto i2 = find_if(i1, query.text.end(), [](char c) { return c == ' ';});
        string_view word(&(*i1), i2 - i1);
        QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                minus_words.push_back(query_word.data);
            } else {
                plus_words.push_back(query_word.data);
            }
        }
        i1 = find_if(i2, query.text.end(), [](char c) { return c != ' ';});
    }

    // number of plus-words larger than number of minus-words
    // sort in parallel only plus-words
    sort(execution::par, plus_words.begin(), plus_words.end());
    plus_words.erase(
        // par version of unique slower than seq
        unique(execution::seq, plus_words.begin(), plus_words.end()),
        plus_words.end());
    query.plus_words.swap(plus_words);

    // use seq version of sort
    sort(execution::seq, minus_words.begin(), minus_words.end());
    minus_words.erase(
        unique(execution::seq, minus_words.begin(), minus_words.end()),
        minus_words.end());;
    query.minus_words.swap(minus_words);

    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(const string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

bool SearchServer::IsValidWord(const string_view word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}
