#include "tcp_connection.hh"

#include <iostream>
#include <cassert>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity()      const { return _sender.stream_in().remaining_capacity(); }
size_t TCPConnection::bytes_in_flight()                  const { return _sender.bytes_in_flight();                }
size_t TCPConnection::unassembled_bytes()                const { return _receiver.unassembled_bytes();            }
size_t TCPConnection::time_since_last_segment_received() const { return _cur_time - _last_recv_time;              }

void TCPConnection::segment_received(const TCPSegment &recv_seg) {
    _last_recv_time = _cur_time;
    const auto &header = recv_seg.header();
    if (header.rst) {
        abort_connection();
        return;
    }
    _receiver.segment_received(recv_seg);
    // passive close
    if (_receiver.stream_out().eof() && !_sender.stream_in().eof()) {
        _linger_after_streams_finish = false;
    }
    // pass ackno and window size to sender, send available data
    if (header.ack) {
        if (_receiver.state() == ReceiverState::LISTEN) {
            return;
        }
        _sender.ack_received(header.ackno, header.win);
        fill_win_send();
    }
    // if received segment occupies any sequence number or it it keep-alive,
    // make sure at least one segment is sent in reply
    auto is_keep_alive = [this](const TCPSegment &seg) -> bool {
        return seg.length_in_sequence_space() == 0
            && seg.header().seqno == _receiver.ackno().value() - 1;
    };
    if (_receiver.ackno().has_value() && (recv_seg.length_in_sequence_space() > 0 || is_keep_alive(recv_seg))) {
        send_ack();
    }
}

bool TCPConnection::active() const {
    bool outbound_active = true;
    if ((_sender.stream_in().eof() && _sender.bytes_in_flight() == 0) || _sender.stream_in().error()) {
        outbound_active = false;
    }
    bool inbound_active = true;
    if (_receiver.stream_out().eof() || _receiver.stream_out().error()) {
        inbound_active = false;
    }
    return inbound_active || outbound_active || _linger_after_streams_finish;
}

size_t TCPConnection::write(const string &data) {
    size_t ret = _sender.stream_in().write(data);
    fill_win_send();
    return ret;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _cur_time += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        send_rst();
        abort_connection();
    } else {
        send_available_segments();
    }
    // active close
    if (_receiver.stream_out().eof() && _sender.stream_in().eof() && _linger_after_streams_finish
            && time_since_last_segment_received() >= 10 * _cfg.rt_timeout) {
        _linger_after_streams_finish = false;
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    fill_win_send();
}

void TCPConnection::connect() {
    fill_win_send();
}

// send syn, fin and data segments
void TCPConnection::fill_win_send() {
    _sender.fill_window();
    send_available_segments();
}

void TCPConnection::send_available_segments() {
    auto &sender_segs = _sender.segments_out();
    while (!sender_segs.empty()) {
        auto &seg = sender_segs.front();
        if (seg.header().syn == false) {
            seg.header().ackno = _receiver.ackno().value();
            seg.header().ack = true;
            seg.header().win = min(_receiver.stream_out().remaining_capacity(),
                    static_cast<size_t>(numeric_limits<uint16_t>::max()));
        }
        _segments_out.push(seg);
        sender_segs.pop();
    }
}

// send ack, rst
void TCPConnection::send_ack() {
    if (_sender.state() == SenderState::CLOSED) {
        // SYN+ACK
        _sender.send_syn();
    } else {
        // ACK
        _sender.send_empty_segment();
    }
    assert(_sender.segments_out().size() == 1);
    TCPSegment &ack_seg = _sender.segments_out().front();
    ack_seg.header().ackno = _receiver.ackno().value();
    ack_seg.header().ack = true;
    ack_seg.header().win = min(_receiver.stream_out().remaining_capacity(),
            static_cast<size_t>(numeric_limits<uint16_t>::max()));
    _segments_out.push(ack_seg);
    _sender.segments_out().pop();
}

void TCPConnection::send_rst() {
    while (!_sender.segments_out().empty()) {
        _sender.segments_out().pop();
    }
    _sender.send_empty_segment();
    assert(_sender.segments_out().size() == 1);
    TCPSegment &seg = _sender.segments_out().front();
    seg.header().rst = true;
    _segments_out.push(seg);
    _sender.segments_out().pop();
}

void TCPConnection::abort_connection() {
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _linger_after_streams_finish = false;
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            // Your code here: need to send a RST segment to the peer
            send_rst();
            abort_connection();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
