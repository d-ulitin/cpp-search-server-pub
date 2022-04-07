#include <algorithm>
#include <execution>
#include <utility>

#include "process_queries.h"

using namespace std;

vector<vector<Document>>
ProcessQueries(
    const SearchServer& search_server,
    const vector<string>& queries) {

    vector<vector<Document>> documents(queries.size());
    transform(
        execution::par,
        queries.begin(), queries.end(), documents.begin(),
        [&search_server](const string& query) {
            return search_server.FindTopDocuments(query);} );
    
    return documents;
}

Iterable2D<vector<vector<Document>>>
ProcessQueriesJoined(
    const SearchServer& search_server,
    const vector<string>& queries) {
    
    vector<vector<Document>> documents = ProcessQueries(search_server, queries);

    return Iterable2D(std::move(documents));
}