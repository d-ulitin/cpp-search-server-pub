#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <cassert>
#include <numeric>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

const double RELEVANCE_EPS = 1e-6;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}
    
struct Document {
    Document() = default;

    Document(int id, double relevance, int rating)
        : id(id)
        , relevance(relevance)
        , rating(rating) {
    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words) {
        for (const string& word : stop_words) {
            if (!IsValidWord(word))
                throw invalid_argument("Stop-word contains invalid character"s);
            if (!word.empty() && stop_words_.count(word) == 0)
                stop_words_.insert(word);
        }
    }
    
    explicit SearchServer(const string& stop_words)
     : SearchServer(SplitIntoWords(stop_words)) {
    }

    SearchServer() = default;

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        if (document_id < 0)
            throw invalid_argument("Document's id is out of range"s);
        if (documents_.count(document_id) > 0)
            throw invalid_argument("Document's id alredy exists"s);
        if (!IsValidWord(document))
            throw invalid_argument("Document contains invalid character"s);
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

    template <typename Filter>
    vector<Document> FindTopDocuments(const string& raw_query, Filter filter) const {            
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, filter);
        
        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < RELEVANCE_EPS) {
                    return lhs.rating > rhs.rating;
                } else {
                    return lhs.relevance > rhs.relevance;
                }
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    // overload FindTopDocuments with no parameters (return actual documents)
    vector<Document> FindTopDocuments(const string& raw_query) const
    {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    // overload FindTopDocuments with status parameter only
    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const
    {
        return FindTopDocuments(raw_query,
            [status](int id, DocumentStatus st, int rating) {
                (void)id;
                (void)rating;
                return st == status;} );
    }

    int GetDocumentCount() const {
        return documents_.size();
    }
    
    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
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

    int GetDocumentId(int index) const {
        // method 'at' throws exception if index is out of range
        return static_cast<int>(document_ids_.at(index));
    }
    
private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    vector<int> document_ids_;
    
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }    

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }
    
    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }
    
    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
        return rating_sum / static_cast<int>(ratings.size());
    }
    
    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };
    
    QueryWord ParseQueryWord(string text) const {
        if (text.empty())
            throw invalid_argument("Empty query word"s);

        bool is_minus = false;

        if (text[0] == '-') {
            if (text.length() < 2)
                throw invalid_argument("Minus-word doesn't contain characters after '-'"s);
            if (text[1] == '-')
                throw invalid_argument("Minus-word starts with '--'"s);
            is_minus = true;
            text = text.substr(1);
        }

        if (!IsValidWord(text))
            throw invalid_argument("Query word contains invalid character"s);

        return {
            text,
            is_minus,
            IsStopWord(text)
        };
    }
    
    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };
    
    Query ParseQuery(const string& text) const {
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
    
    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename Filter>
    vector<Document> FindAllDocuments(const Query& query, Filter filter) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
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
        
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
            });
        }
        return matched_documents;
    }

    static bool IsValidWord(const string& word) {
        // A valid word must not contain special characters
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
        });
    }
};

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}


// -------- Начало модульных тестов поисковой системы ----------

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename Test>
void RunTestImpl(Test func, const string& func_name) {
    func();
    cerr << func_name << " OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl(func, #func)


// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

void TestAddDocument() {
    int docs_count = 0;
    int first_doc_id = 13;
    SearchServer server;
    ASSERT_EQUAL(server.GetDocumentCount(), 0);
    server.AddDocument(first_doc_id, "one two three four five"s, DocumentStatus::ACTUAL, {1,2,3});
    ++docs_count;
    ASSERT_EQUAL(server.GetDocumentCount(), docs_count);
    {
        vector<Document> docs = server.FindTopDocuments("one");
        ASSERT_EQUAL(docs.size(), 1u);
        const Document& d = docs[0];
        ASSERT_EQUAL(d.id, first_doc_id);
        ASSERT(abs(d.relevance) < RELEVANCE_EPS);
        ASSERT_EQUAL(d.rating, 2); 
    }
    {
        vector<Document> docs = server.FindTopDocuments("five");
        ASSERT_EQUAL(docs.size(), 1u);
        const Document& d = docs[0];
        ASSERT_EQUAL(d.id, first_doc_id);
        ASSERT(abs(d.relevance) < RELEVANCE_EPS);
        ASSERT_EQUAL(d.rating, 2); 
    }
    {
        vector<Document> docs = server.FindTopDocuments("six");
        ASSERT_EQUAL(docs.size(), 0u);
    }
    // test adding some more documents
    for (int i = 0; i < 3; ++i) {
        int doc_id = first_doc_id + i + 1;
        auto content = to_string(doc_id);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, {1});
        ++docs_count;
        auto docs = server.FindTopDocuments(content);
        ASSERT_EQUAL(docs.size(), 1u);
        ASSERT_EQUAL(docs[0].id, doc_id);
    }
    ASSERT_EQUAL(server.GetDocumentCount(), docs_count);
}

void TestStopWords() {
    int doc_id = 13;
    SearchServer server("a and not"s);
    ASSERT_EQUAL(server.GetDocumentCount(), 0);
    server.AddDocument(doc_id, "not one and two three four five"s, DocumentStatus::ACTUAL, {1,2,3});
    ASSERT_EQUAL(server.GetDocumentCount(), 1);
    {
        vector<Document> docs = server.FindTopDocuments("three");
        ASSERT_EQUAL(docs.size(), 1u);
        const Document& d = docs[0];
        ASSERT(abs(d.relevance) < RELEVANCE_EPS);
        ASSERT_EQUAL(d.id, doc_id);
    }
    {
        vector<Document> docs = server.FindTopDocuments("and");
        ASSERT_EQUAL(docs.size(), 0u);
    }
}

void TestMinusWords() {
    int doc_id = 13;
    SearchServer server("a and not"s);
    ASSERT_EQUAL(server.GetDocumentCount(), 0);
    server.AddDocument(doc_id, "not one and two three four five"s, DocumentStatus::ACTUAL, {1,2,3});
    ASSERT_EQUAL(server.GetDocumentCount(), 1);
    {
        vector<Document> docs = server.FindTopDocuments("two"s);
        ASSERT_EQUAL(docs.size(), 1u);
        const Document& d = docs[0];
        ASSERT_EQUAL(d.id, doc_id);
        ASSERT_EQUAL(d.rating, 2);
        ASSERT(abs(d.relevance) < RELEVANCE_EPS);
    }
    {
        vector<Document> docs = server.FindTopDocuments("two -three five"s);
        ASSERT_EQUAL(docs.size(), 0u);
    }
    {
        vector<Document> docs = server.FindTopDocuments("two -and"s);
        ASSERT_EQUAL(docs.size(), 1u);
        const Document& d = docs[0];
        ASSERT_EQUAL(d.id, doc_id);
        ASSERT_EQUAL(d.rating, 2);
        ASSERT(abs(d.relevance) < RELEVANCE_EPS);
    }
}

void TestMatchDocument2() {
    int doc_id = 13;
    SearchServer server("a and not"s);
    ASSERT_EQUAL(server.GetDocumentCount(), 0);
    server.AddDocument(doc_id, "not one and two three four five"s, DocumentStatus::ACTUAL, {1,2,3});
    ASSERT_EQUAL(server.GetDocumentCount(), 1);
    {
        auto match = server.MatchDocument("and one two"s, doc_id);
        ASSERT_EQUAL(get<0>(match).size(), 2u);
        ASSERT_EQUAL(get<0>(match).at(0), "one"s);
        ASSERT_EQUAL(get<0>(match).at(1), "two"s);
        ASSERT(get<1>(match) == DocumentStatus::ACTUAL);
    }
    {
        auto match = server.MatchDocument("one two -three"s, doc_id);
        ASSERT_EQUAL(get<0>(match).size(), 0u);
    }
}

void TestRelevanceSort() {
    SearchServer server("a and not"s);
    server.AddDocument(1, "not xxx one and xxx two three four five yyy"s, DocumentStatus::ACTUAL, {1,2,3});
    server.AddDocument(2, "not yyy yyy one and two xxx three four five yyy"s, DocumentStatus::ACTUAL, {1,2,3});
    server.AddDocument(3, "xxx not one and two three xxx four five xxx yyy yyy"s, DocumentStatus::ACTUAL, {1,2,3});
    ASSERT_EQUAL(server.GetDocumentCount(), 3);
    {
        vector<Document> docs = server.FindTopDocuments("xxx"s);
        ASSERT_EQUAL(docs.size(), 3u);
        ASSERT(docs[0].relevance >= docs[1].relevance && docs[1].relevance >= docs[2].relevance);
    }
    {
        vector<Document> docs = server.FindTopDocuments("yyy"s);
        ASSERT_EQUAL(docs.size(), 3u);
        ASSERT(docs[1].relevance >= docs[2].relevance && docs[2].relevance >= docs[0].relevance);
    }
}

void TestDocumentRating() {
    SearchServer server("a and not"s);
    server.AddDocument(1, "not one"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(2, "not two"s, DocumentStatus::ACTUAL, {1,2,3});
    server.AddDocument(3, "not five"s, DocumentStatus::ACTUAL, {5,5,5});
    ASSERT_EQUAL(server.GetDocumentCount(), 3);
    {
        vector<Document> docs = server.FindTopDocuments("one"s);
        ASSERT_EQUAL(docs.size(), 1u);
        ASSERT_EQUAL(docs[0].rating, 1);
    }
    {
        vector<Document> docs = server.FindTopDocuments("two"s);
        ASSERT_EQUAL(docs.size(), 1u);
        ASSERT_EQUAL(docs[0].rating, 2);
    }
    {
        vector<Document> docs = server.FindTopDocuments("five"s);
        ASSERT_EQUAL(docs.size(), 1u);
        ASSERT_EQUAL(docs[0].rating, 5);
    }
}

void TestUserPredicate() {
    SearchServer server;
    server.AddDocument(1, "xxx one"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(2, "xxx two"s, DocumentStatus::BANNED, {2});
    server.AddDocument(3, "xxx three"s, DocumentStatus::IRRELEVANT, {3});
    ASSERT_EQUAL(server.GetDocumentCount(), 3);
    {
        auto docs = server.FindTopDocuments("xxx"s, [](int id, DocumentStatus st, int rating) {
                (void)st;
                (void)rating;
                return id == 1;} );
        ASSERT_EQUAL(docs.size(), 1u);
        ASSERT_EQUAL(docs[0].id, 1);
    }
    {
        auto docs = server.FindTopDocuments("xxx"s, [](int id, DocumentStatus st, int rating) {
                (void)id;
                (void)rating;
                return st == DocumentStatus::BANNED;} );
        ASSERT_EQUAL(docs.size(), 1u);
        ASSERT_EQUAL(docs[0].id, 2);
    }
    {
        auto docs = server.FindTopDocuments("xxx"s, [](int id, DocumentStatus st, int rating) {
                (void)id;
                (void)st;
                return rating == 3;} );
        ASSERT_EQUAL(docs.size(), 1u);
        ASSERT_EQUAL(docs[0].rating, 3);
    }
}

void TestStatusFilter() {
    SearchServer server;
    server.AddDocument(1, "xxx actual"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(2, "xxx banned"s, DocumentStatus::BANNED, {2});
    server.AddDocument(3, "xxx irrelevant"s, DocumentStatus::IRRELEVANT, {3});
    server.AddDocument(4, "xxx removed"s, DocumentStatus::REMOVED, {4});
    ASSERT_EQUAL(server.GetDocumentCount(), 4);
    {
        auto docs = server.FindTopDocuments("xxx"s, DocumentStatus::ACTUAL);
        ASSERT_EQUAL(docs.size(), 1u);
        ASSERT_EQUAL(docs[0].id, 1);
    }
    {
        auto docs = server.FindTopDocuments("xxx"s, DocumentStatus::BANNED);
        ASSERT_EQUAL(docs.size(), 1u);
        ASSERT_EQUAL(docs[0].id, 2);
    }
    {
        auto docs = server.FindTopDocuments("xxx"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(docs.size(), 1u);
        ASSERT_EQUAL(docs[0].id, 3);
    }
    {
        auto docs = server.FindTopDocuments("xxx"s, DocumentStatus::REMOVED);
        ASSERT_EQUAL(docs.size(), 1u);
        ASSERT_EQUAL(docs[0].id, 4);
    }
}

void TestRelevanceValue() {
    SearchServer server;
    server.AddDocument(1, "xxx xxx one two three four five"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(2, "xxx one two three four five"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(3, "xxx xxx xxx one two three four five"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(4, "yyy zzz"s, DocumentStatus::ACTUAL, {1});
    ASSERT_EQUAL(server.GetDocumentCount(), 4);
    {
        vector<Document> docs = server.FindTopDocuments("xxx"s);
        ASSERT_EQUAL(docs.size(), 3u);
        // calc TF-IDF manually
        // relevance = TF * IDF
        // TF = the_word_number_in_document / total_words_number_in_document
        // IDF = log(total_number_of_documents / number_of_documents_with_the_word)
        ASSERT(abs(docs.at(0).relevance - (3.0/8.0) * log(4.0/3.0)) < RELEVANCE_EPS);
        ASSERT(abs(docs.at(1).relevance - (2.0/7.0) * log(4.0/3.0)) < RELEVANCE_EPS);
        ASSERT(abs(docs.at(2).relevance - (1.0/6.0) * log(4.0/3.0)) < RELEVANCE_EPS);
    }
    {
        vector<Document> docs = server.FindTopDocuments("yyy"s);
        ASSERT_EQUAL(docs.size(), 1u);
        ASSERT(abs(docs.at(0).relevance - (1.0/2.0) * log(4.0/1.0)) < RELEVANCE_EPS);
    }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddDocument);
    RUN_TEST(TestStopWords);
    RUN_TEST(TestMinusWords);
    RUN_TEST(TestMatchDocument2);
    RUN_TEST(TestRelevanceSort);
    RUN_TEST(TestDocumentRating);
    RUN_TEST(TestUserPredicate);
    RUN_TEST(TestStatusFilter);
    RUN_TEST(TestRelevanceValue);
}

// --------- Окончание модульных тестов поисковой системы -----------

void PrintMatchDocumentResult(int document_id, const vector<string>& words, DocumentStatus status) {
    cout << "{ "s
         << "document_id = "s << document_id << ", "s
         << "status = "s << static_cast<int>(status) << ", "s
         << "words ="s;
    for (const string& word : words) {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}

void AddDocument(SearchServer& search_server, int document_id, const string& document, DocumentStatus status,
                 const vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    } catch (const exception& e) {
        cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
    cout << "Результаты поиска по запросу: "s << raw_query << endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    } catch (const exception& e) {
        cout << "Ошибка поиска: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const string& query) {
    try {
        cout << "Матчинг документов по запросу: "s << query << endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index) {
            const int document_id = search_server.GetDocumentId(index);
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    } catch (const exception& e) {
        cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << endl;
    }
}

int main() {
    // TestSearchServer();

    SearchServer search_server("и в на"s);

    AddDocument(search_server, 1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    AddDocument(search_server, 1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
    AddDocument(search_server, -1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
    AddDocument(search_server, 3, "большой пёс скво\x12рец евгений"s, DocumentStatus::ACTUAL, {1, 3, 2});
    AddDocument(search_server, 4, "большой пёс скворец евгений"s, DocumentStatus::ACTUAL, {1, 1, 1});

    FindTopDocuments(search_server, "пушистый -пёс"s);
    FindTopDocuments(search_server, "пушистый --кот"s);
    FindTopDocuments(search_server, "пушистый -"s);

    MatchDocuments(search_server, "пушистый пёс"s);
    MatchDocuments(search_server, "модный -кот"s);
    MatchDocuments(search_server, "модный --пёс"s);
    MatchDocuments(search_server, "пушистый - хвост"s);
} 