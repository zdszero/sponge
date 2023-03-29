#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    DUMMY_CODE(seg);
    const auto &header = seg.header();
    if (header.syn == true) {
        if (_syn_recv) {
            return;
        }
        _isn = header.seqno;
        _syn_recv = true;
    }
    if (!_syn_recv) {
        return;
    }
    uint64_t stream_index = unwrap(header.seqno + header.syn, _isn + 1, _reassembler.expected_stream_index());
    _reassembler.push_substring(seg.payload().copy(), stream_index, header.fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_syn_recv) {
        return {};
    }
    return wrap(_reassembler.expected_stream_index(), _isn) + _syn_recv + stream_out().input_ended();
}

size_t TCPReceiver::window_size() const { return _reassembler.stream_out().remaining_capacity(); }

ReceiverState TCPReceiver::state() const {
    if (!_syn_recv) {
        return ReceiverState::LISTEN;
    }
    if (stream_out().input_ended()) {
        return FIN_RECV;
    } else {
        return SYN_RECV;
    }
}
