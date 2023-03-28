#include "byte_stream.hh"

#include <cassert>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _buffer(capacity), _capacity(capacity) {}

size_t ByteStream::write(const string &data) {
    size_t bytes_written = min(_capacity - _size, data.size());
    for (size_t i = 0; i < bytes_written; i++) {
        _buffer[_size + i] = data[i];
    }
    _size += bytes_written;
    _bytes_written += bytes_written;
    return bytes_written;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const { return std::string(&_buffer[0], len); }

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    assert(len <= _size);
    _size -= len;
    for (size_t i = 0; i < _size; i++) {
        _buffer[i] = _buffer[i + len];
    }
    _bytes_read += len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    std::string read_str = peek_output(len);
    pop_output(len);
    return read_str;
}

void ByteStream::end_input() { _end = true; }

bool ByteStream::input_ended() const { return _end; }

size_t ByteStream::buffer_size() const { return _size; }

bool ByteStream::buffer_empty() const { return _size == 0; }

bool ByteStream::eof() const { return buffer_empty() && input_ended() && !error(); }

size_t ByteStream::bytes_written() const { return _bytes_written; }

size_t ByteStream::bytes_read() const { return _bytes_read; }

size_t ByteStream::remaining_capacity() const { return _capacity - _size; }
