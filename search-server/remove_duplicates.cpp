#include <set>
#include <string>
#include <vector>
#include <iostream>
#include <utility>

#include "remove_duplicates.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
    set<set<string>> unique_docs;
    vector<int> docs_to_delete;
    for (int document_id : search_server) {
        map<string, double> word_freqs = search_server.GetWordFrequencies(document_id);
        set<string> doc_words;
        for (const pair<string, double> wf: word_freqs) {
            doc_words.insert(wf.first);
        }
        if (unique_docs.count(doc_words) > 0) {
            // duplicate
            docs_to_delete.push_back(document_id);
            cout << "Found duplicate document id "s << document_id << endl;
        } else {
            // unique
            unique_docs.insert(doc_words);
        }
    }
    for (auto id : docs_to_delete) {
        search_server.RemoveDocument(id);
    }
}