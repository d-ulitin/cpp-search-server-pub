#include "request_queue.h"

using namespace std;

RequestQueue::RequestQueue(const SearchServer& search_server)
    : search_server_(search_server) {
}

vector<Document>
RequestQueue::AddFindRequest(const string& raw_query, DocumentStatus status) {
    return AddQueryResult(search_server_.FindTopDocuments(raw_query, status));
}

vector<Document>
RequestQueue::AddFindRequest(const string& raw_query) {
    return AddQueryResult(search_server_.FindTopDocuments(raw_query));
}

int RequestQueue::GetNoResultRequests() const {
    return no_result_requests_;
}

vector<Document>
RequestQueue::AddQueryResult(const vector<Document> documents) {
    ++current_time_;

    while (!requests_.empty() && min_in_day_ <= current_time_ - requests_.front().timestamp) {
        if (requests_.front().results == 0) {
            --no_result_requests_;
        }
        requests_.pop_front();
    }
    requests_.push_back(QueryResult{current_time_, documents.size()});
    if (documents.size() == 0)
        ++no_result_requests_;

    return documents;
}