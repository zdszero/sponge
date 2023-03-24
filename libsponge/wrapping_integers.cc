#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    DUMMY_CODE(n, isn);
    return WrappingInt32{static_cast<uint32_t>(n) + isn.raw_value()};
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    DUMMY_CODE(n, isn, checkpoint);
    // steps in [-2^32, 2^32-1]
    int32_t steps = n - wrap(checkpoint, isn);
    int64_t res = checkpoint + steps;
    return res >= 0 ? res : res + (1UL << 32);
    // int64_t round = (1l << 32);
    // int64_t n64 = static_cast<int64_t>(n.raw_value());
    // int64_t isn64 = static_cast<int64_t>(isn.raw_value());
    // int64_t moves = (n64 + round - isn64) % round;
    // int64_t guess1 = moves + round * (checkpoint / round);
    // int64_t guess2;
    // int64_t check64 = static_cast<int64_t>(checkpoint);
    // if (guess1 == check64) {
    //     return checkpoint;
    // } else if (guess1 < check64) {
    //     guess2 = guess1 + round;
    // } else {
    //     guess2 = guess1;
    //     guess1 = guess2 - round;
    // }
    // if (guess1 < 0) {
    //     return static_cast<uint64_t>(guess2);
    // }
    // if (guess2 - check64 <= check64 - guess1) {
    //     return static_cast<uint64_t>(guess2);
    // }
    // return static_cast<uint64_t>(guess1);
}
