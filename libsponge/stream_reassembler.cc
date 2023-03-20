#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    bool all_data_pushed = true;
    if (data.size() > 0) {
        size_t first_unread = _first_unassembled - _output.buffer_size();
        // ignore duplicate string
        if (index + data.size() <= _first_unassembled) {
        } else if (index >= first_unread + _capacity) {
        } else if (index > _first_unassembled) {
            // not the expecting next segment
            if (index + data.size() > first_unread + _capacity) {
                all_data_pushed = false;
                // _cached_data[index] = data.substr(0, first_unread + _capacity - index);
                caching_data(index, data.substr(0, first_unread + _capacity - index));
            } else {
                // _cached_data[index] = data;
                caching_data(index, data);
            }
        } else {
            size_t data_begin = _first_unassembled - index;
            size_t data_len = min(data.size() - data_begin, first_unread + _capacity - _first_unassembled);
            if (data_len + data_begin < data.size()) {
                all_data_pushed = false;
            }
            _output.write(data.substr(data_begin, data_len));
            _first_unassembled += data_len;
        }
    }
    if (eof && all_data_pushed) {
        _pending_eof = true;
    }
    push_cached_data();
}

void StreamReassembler::caching_data(size_t start, const std::string &s) {
    if (_cached_data.count(start)) {
        auto it = _cached_data.find(start);
        if (s.size() > it->second.size()) {
            it->second = s;
        }
        return;
    }
    _cached_data[start] = s;
    for (auto it = _cached_data.begin(); it != _cached_data.end() && std::next(it) != _cached_data.end();) {
        auto it2 = std::next(it);
        // combine [a, b) [c, d)
        size_t a = it->first;
        size_t b = it->first + it->second.size();
        size_t c = it2->first;
        size_t d = it2->first + it2->second.size();
        if (c <= b) {
            if (d > b) {
                std::string comb_str = "";
                comb_str += it->second.substr(0, c - a);
                comb_str += it2->second;
                it->second = comb_str;
            }
            _cached_data.erase(it2);
        } else {
            it++;
        }
    }
}

void StreamReassembler::push_cached_data() {
    for (auto it = _cached_data.begin(); it != _cached_data.end();) {
        if (it->first + it->second.size() <= _first_unassembled) {
        } else if (it->first > _first_unassembled) {
            break;
        } else {
            size_t data_begin = _first_unassembled - it->first;
            _output.write(it->second.substr(data_begin));
            _first_unassembled = it->first + it->second.size();
        }
        it = _cached_data.erase(it);
    }
    if (_cached_data.empty() && _pending_eof) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t cached_bytes = 0;
    size_t start = 0, end = 0;
    for (auto it = _cached_data.begin(); it != _cached_data.end(); it++) {
        if (it->first <= end) {
            end = max(end, it->first + it->second.size());
        } else {
            cached_bytes += (end - start);
            start = it->first;
            end = it->first + it->second.size();
        }
    }
    cached_bytes += (end - start);
    return cached_bytes;
}

bool StreamReassembler::empty() const { return _cached_data.empty(); }
