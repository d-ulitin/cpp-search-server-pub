#include "testing.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

#include "search_server.h"

using namespace std;

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

