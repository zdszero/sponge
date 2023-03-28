#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <cassert>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _rto(retx_timeout)
    , _timer(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const {
    uint64_t ans = 0;
    for (auto it = _outgoing_segments.begin(); it != _outgoing_segments.end(); it++) {
        ans += it->second.length_in_sequence_space();
    }
    return ans;
}

void TCPSender::fill_window() {
    // buf_size, win, MAX_PAYLOAD_SIZE
    if (!_syn_sent) {
        send_empty_segment();
        return;
    }
    if (!_sync) {
        return;
    }
    if (_win == 0 && _outgoing_segments.empty()) {
        TCPSegment keep_alive;
        if (_stream.buffer_size() > 0 || _stream.input_ended()) {
            keep_alive.header().seqno = wrap(_next_seqno, _isn);
            if (_stream.buffer_size() > 0) {
                keep_alive.payload() = Buffer(_stream.read(1));
            } else if (_stream.input_ended()) {
                keep_alive.header().fin = true;
                _fin_sent = true;
            }
            _outgoing_segments[_next_seqno] = keep_alive;
            _timer.start();
        } else {
            keep_alive.header().seqno = wrap(_next_seqno - 1, _isn);
        }
        _next_seqno += keep_alive.length_in_sequence_space();
        _segments_out.push(keep_alive);
        return;
    }
    while (_win > 0 && _stream.buffer_size() > 0) {
        TCPSegment seg;
        seg.header().seqno = wrap(_next_seqno, _isn);
        size_t payload_len = min(static_cast<size_t>(_win),
                min(TCPConfig::MAX_PAYLOAD_SIZE, _stream.buffer_size()));
        seg.payload() = Buffer(_stream.read(payload_len));
        _win -= payload_len;
        // piggyback FIN
        if (_stream.input_ended() && !_fin_sent && _win > 0) {
            seg.header().fin = true;
            _win -= 1;
            _fin_sent = true;
        }
        _segments_out.push(seg);
        _next_seqno += seg.length_in_sequence_space();
        _outgoing_segments[_next_seqno] = seg;
        _timer.start();
    }
    if (_stream.input_ended() && !_fin_sent && _win > bytes_in_flight()) {
        TCPSegment fin_seg;
        fin_seg.header().fin = true;
        fin_seg.header().seqno = wrap(_next_seqno, _isn);
        _segments_out.push(fin_seg);
        _next_seqno += 1;
        _outgoing_segments[_next_seqno] = fin_seg;
        _timer.start();
        _fin_sent = true;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t recv_absno = unwrap(ackno, _isn, _next_seqno);
    // with same ackno, window size can be reset, but timer cannot be reset
    if (recv_absno >= _lastest_ack_absno) {
        _reported_win = window_size;
        _win = window_size;
    }
    if (recv_absno <= _lastest_ack_absno) {
        return;
    }
    // only do the following operations with ack on new data
    _rto = _initial_retransmission_timeout;
    _lastest_ack_absno = recv_absno;
    for (auto it = _outgoing_segments.begin(); it != _outgoing_segments.end();) {
        bool syn = it->second.header().syn;
        bool fin = it->second.header().fin;
        size_t seg_begin = unwrap(it->second.header().seqno, _isn, _next_seqno);
        if ((syn || fin) && recv_absno != it->first) {
            break;
        }
        if (recv_absno >= it->first) {
            it = _outgoing_segments.erase(it);
            if (syn) {
                _sync = true;
            }
            if (syn || fin) {
                assert(_outgoing_segments.empty());
            }
        } else if (recv_absno > seg_begin) {
            it->second.payload().set_start_offset(recv_absno - seg_begin);
            break;
        } else {
            break;
        }
    }
    // reset timer whatsoever
    _timer.reset(_rto);
    if (_outgoing_segments.empty()) {
        _timer.stop();
    }
    _consecutive_retransmissions = 0;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    // stop timer when all data has been acked
    _timer.tick(ms_since_last_tick);
    if (_timer.is_timeout()) {
        if (!_outgoing_segments.empty()) {
            TCPSegment seg = _outgoing_segments.begin()->second;
            _segments_out.push(seg);
        }
        if (_reported_win > 0) {
            _consecutive_retransmissions++;
            _rto *= 2;
        }
        _timer.reset(_rto);
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    if (!_syn_sent) {
        assert(_next_seqno == 0);
        seg.header().syn = true;
        _syn_sent = true;
        _next_seqno += 1;
        _outgoing_segments[_next_seqno] = seg;
        _timer.start();
    }
    _segments_out.push(seg);
}
