#pragma once

#include <cstdlib>
#include <deque>
#include <string>
#include <vector>

#include "document.h"
#include "search_server.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;

private:
    struct QueryResult {
        int timestamp;
        size_t results;
    };
    
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    const SearchServer& search_server_;
    int no_result_requests_ = 0;
    int current_time_;

    std::vector<Document> AddQueryResult(const std::vector<Document> documents);
};

template <typename DocumentPredicate>
std::vector<Document>
RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    return AddQueryResult(raw_query, search_server_.FindTopDocuments(raw_query, document_predicate));
}
