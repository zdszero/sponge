#include "tcp_connection.hh"
#include "util.hh"

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
size_t TCPConnection::time_since_last_segment_received() const { return timestamp_ms() - _last_time;              }

void TCPConnection::segment_received(const TCPSegment &recv_seg) {
    _last_time = timestamp_ms();
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
        _sender.ack_received(header.ackno, header.win);
        send_data_segments();
    }
    // if received segment occupies any sequence number or it it keep-alive,
    // make sure at least one segment is sent in reply
    auto is_keep_alive = [this](const TCPSegment &seg) -> bool {
        return seg.length_in_sequence_space() == 0
            && seg.header().seqno == _receiver.ackno().value() - 1;
    };
    if (_receiver.ackno().has_value() && (recv_seg.length_in_sequence_space() > 0 || is_keep_alive(recv_seg))) {
        send_reply_segment([this](TCPSegment &seg) {
            seg.header().ackno = _receiver.ackno().value();
            seg.header().ack = true;
            seg.header().win = min(remaining_outbound_capacity(),
                    static_cast<size_t>(numeric_limits<uint16_t>::max()));
        });
    }
}

bool TCPConnection::active() const {
    bool in_active = (!_sender.stream_in().eof() && !_sender.stream_in().error());
    bool out_active = (!_receiver.stream_out().eof() && !_receiver.stream_out().error());
    return in_active || out_active || _linger_after_streams_finish;
}

size_t TCPConnection::write(const string &data) {
    size_t ret = _sender.stream_in().write(data);
    send_data_segments();
    return ret;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        abort_connection();
        send_reply_segment([](TCPSegment &seg) {
            seg.header().rst = true;
        });
    }
    // positive close
    if (_receiver.stream_out().eof() && _sender.stream_in().eof() && _linger_after_streams_finish
            && time_since_last_segment_received() > 10 * _cfg.rt_timeout) {
        _linger_after_streams_finish = false;
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
}

void TCPConnection::connect() {
    send_data_segments();
}

// send syn, fin and data segments
void TCPConnection::send_data_segments() {
    _sender.fill_window();
    auto &sender_segs = _sender.segments_out();
    while (!sender_segs.empty()) {
        _segments_out.push(sender_segs.front());
        sender_segs.pop();
    }
}

// send ack, rst
void TCPConnection::send_reply_segment(std::function<void(TCPSegment &esg)> fn) {
    _sender.send_empty_segment();
    assert(_sender.segments_out().size() == 1);
    TCPSegment &ack_seg = _sender.segments_out().front();
    fn(ack_seg);
    _segments_out.push(ack_seg);
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
            send_reply_segment([](TCPSegment &seg) {
                seg.header().rst = true;
            });
            abort_connection();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
