#pragma once
#include <algorithm>
#include <vector>
#include <cassert>
#include <iostream>
#include "document.h"

using namespace std::string_literals;

template <typename IteratorRanges>
class IteratorRange {
public:
    explicit IteratorRange(IteratorRanges begin, IteratorRanges end) : begin_(begin), end_(end), size_(distance(begin, end)) {}
    IteratorRanges begin() const {
        return begin_;
    }
    IteratorRanges end() const {
        return end_;
    }
    size_t size() const {
        return size_;
    }
private:
    IteratorRanges begin_;
    IteratorRanges end_;
    size_t size_;
};

std::ostream& operator<<(std::ostream& out, const Document& document) {
    out << "{ document_id = "s << document.id
        << ", relevance = "s << document.relevance
        << ", rating = "s << document.rating << " }"s;
    return out;
}

template <typename To_Out>
std::ostream& operator<<(std::ostream& out, const IteratorRange<To_Out>& sheet) {
    auto helper = sheet.begin();
    while (helper != sheet.end()) {
        out << *helper;
        ++helper;
    }
    return out;
}

template <typename Paginatorr>
class Paginator {
public:
    Paginator(const Paginatorr& result_begin, const Paginatorr& result_end, size_t size_of_sheet) {
        auto full_size = distance(result_begin, result_end);
        Paginatorr helper = result_begin;
        for (auto i = 0; i < full_size / size_of_sheet; ++i) {
            sheets.push_back(IteratorRange<Paginatorr>(helper, helper + size_of_sheet));
            helper = helper + size_of_sheet;
        }
        if (helper != result_end) {
            sheets.push_back(IteratorRange<Paginatorr>(helper, result_end));
        }
    }
    auto begin() const {
        return sheets.begin();
    }
    auto end() const {
        return sheets.end();
    }
    size_t size() {
        return sheets.size();
    }
private:
    std::vector<IteratorRange<Paginatorr>> sheets;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}
