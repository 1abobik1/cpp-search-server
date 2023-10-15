#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <numeric>

using namespace std;

constexpr double EPSILON = 1e-6;
constexpr int MAX_RESULT_DOCUMENT_COUNT = 5;

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
            words.push_back(word);
            word = "";
        }
        else {
            word += c;
        }
    }
    words.push_back(word);

    return words;
}

struct Document {

    Document() = default;

    Document(int id_, double relevance_, int rating_) : id(id_), relevance(relevance_), rating(rating_)
    {

    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    set<string> non_empty_strings;
    for (const string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}


enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:

    inline static constexpr int INVALID_DOCUMENT_ID = -1;

    SearchServer() = default;

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
	{
        if (all_of(stop_words.begin(), stop_words.end(), IsValidWord)) {}

    	else
        {
            throw invalid_argument("It is forbidden to use special characters"s);
        }
    }

    explicit SearchServer(const string& stop_words_text)
        : SearchServer(
            SplitIntoWords(stop_words_text)) 
    {
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        if(document_id < 0)
        {
            throw invalid_argument("Trying to add a document with a negative id"s);
        }

        if(documents_.find(document_id) != documents_.end())
        {
            throw invalid_argument("An attempt to add a document with the id of a previously added document;"s);
        }

    	const vector<string> words = SplitIntoWordsNoStop(document);
        for (auto& word : words) {
            if (!IsValidWord(word)) {
                throw invalid_argument("It is forbidden to use special characters"s);
            }
        }
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += 1.0 / static_cast<double>(words.size());
        }
        documents_.emplace(document_id,
            DocumentData{
                ComputeAverageRating(ratings),
                status
            });
        documents_index_.push_back(document_id);
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {

        if(raw_query.empty())
        {
            throw invalid_argument("Query empty "s);
        }

        for (const string& word : SplitIntoWords(raw_query))
        {
            if (!IsValidWord(word))
            {
                throw invalid_argument("It is forbidden to use special characters "s);
            }
            if (!IsValidMinusWord(word))
            {
                throw invalid_argument("Presence of more than one minus sign before words or missing text after the minus symbol "s);
            }
        }

    	const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
                    return lhs.rating > rhs.rating;
                }
            	return lhs.relevance > rhs.relevance;
                
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
        
    }

	vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(raw_query, [&status](int id, const DocumentStatus& doc_status, int raring)
        {
	        return doc_status == status;
        });
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return static_cast<int>(documents_.size());
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        if(raw_query.empty())
        {
            throw invalid_argument("Query empty "s);
        }

        for(const string& word : SplitIntoWords(raw_query))
        {
	        if(!IsValidWord(word))
	        {
                throw invalid_argument("It is forbidden to use special characters "s);
	        }
            if(!IsValidMinusWord(word))
            {
                throw invalid_argument("Presence of more than one minus sign before words or missing text after the minus symbol "s);
            }
        }

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

        return { matched_words, documents_.at(document_id).status };
        
    }

    int GetDocumentId(const int& index) const 
    {
        if ((index) >= 0 && (index < GetDocumentCount()))
        {
            return documents_index_[index];
        }
    	throw out_of_range("The index of the transmitted document is out of range (0; number of documents )"s);
    }


private:


    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    vector<int> documents_index_;

    static bool IsValidWord(const string& word) {
        // A valid word must not contain special characters
        return none_of(word.begin(),word.end(), [](const char c) {
            return c >= '\0' && c < ' ';
            });
    }

    static bool IsValidMinusWord(const string& word)
    {
        if(word.empty())
        {
            return false;
        }

    	if(word == "-"s)
	    {
            return false;
	    }
        if(word[0] == '-' && word[1] == '-')
        {
            return false;
        }

        return true;
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

    static int ComputeAverageRating(const std::vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }

        const int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        if(text.empty())
        {
            throw invalid_argument("Query empty "s);
        }

    	bool is_minus = false;
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }

        if(text.empty())
        {
            throw invalid_argument(R"(Space or no text after the "-" sign )");
        }

        if(text[-1] == '-')
        {
	        throw invalid_argument(R"(Double "-" sign in negative keyword )");
        }

        if(!IsValidWord(text))
        {
	        throw invalid_argument("It is forbidden to use special characters "s);
        }

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
                }
                else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / static_cast<double>(word_to_document_freqs_.at(word).size()));
    } 

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
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
        matched_documents.reserve(document_to_relevance.size());
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.emplace_back(
	            document_id,
                relevance,
                documents_.at(document_id).rating
            );
        }
        return matched_documents;
    }
};

void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s << endl;
}

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

void AddDocument(SearchServer& search_server, int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
	try
	{
        search_server.AddDocument(document_id, document, status, ratings);
	}
	catch (const invalid_argument& exp_exception)
	{
        cerr << "Error adding document "s << document_id << ": "s << exp_exception.what() << endl;
	}
}

void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
    cout <<"Search result for the query : "s << raw_query << endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    }
    catch (const invalid_argument& exp_exception) {
        cerr << "Search error: "s << exp_exception.what() << endl;
    }
}

void  MatchDocuments(const SearchServer& search_server, const string& query) {
    try {
        cout << "Match documents on request: "s << query << endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index) {
            const int document_id = search_server.GetDocumentId(index);
            auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    }
    catch (const exception& exp_exception) {
        cerr << "Error matching documents for request "s << query << ": "s << exp_exception.what() << endl;
    }
}

int main(){
    setlocale(LC_ALL, "RU");
	SearchServer search_server("и в на% /"s);
    AddDocument(search_server, 1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 8, 2, 3, 4 });
    AddDocument(search_server, 2, "пушистый пес и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 ,3, 4 });
    AddDocument(search_server, -1, "пушистый пес и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2, 3, 4 });
    AddDocument(search_server, 2, "большой пушистый кот большой пес"s, DocumentStatus::ACTUAL, { 1, 2 ,3, 4, 5 });
    AddDocument(search_server, 3, "большой пес скво\x12рец евгений"s, DocumentStatus::ACTUAL, { 1, 3, 2, 4, 5 });
    AddDocument(search_server, 4, "большой пушистый пес и пушистый кот"s, DocumentStatus::ACTUAL, { 1, 2, 3, 4, 5 });
    AddDocument(search_server, 5, "пушистый кот пушистый кот-пес"s, DocumentStatus::ACTUAL, { 8, 2, 3, 4 });
    FindTopDocuments(search_server, "пушистый -пес"s);
    FindTopDocuments(search_server, "пушистый -кот"s);
    FindTopDocuments(search_server, "пушистый кот"s);
    FindTopDocuments(search_server, "пушистый --кот"s);
    FindTopDocuments(search_server, "пушистый -"s);
    FindTopDocuments(search_server, "кот-пес"s);
    FindTopDocuments(search_server, "пушистый\x12 -"s);

    MatchDocuments(search_server, "пушистый пес"s);
    MatchDocuments(search_server, "кот -пушистый"s);
    MatchDocuments(search_server, "модный --пес"s);
    MatchDocuments(search_server, "пушистый - хвост"s);
    MatchDocuments(search_server, "пушистый  хвост\x12"s);
}
