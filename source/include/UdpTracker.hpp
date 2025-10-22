#pragma once

#include <BaseTracker.hpp>
#include <Utils.hpp> // for parse_url if you have it

#include <boost/asio.hpp>

#include <iostream>
#include <random>
#include <chrono>
#include <future>

using udp = boost::asio::ip::udp;

static inline uint32_t rand32() {
    static std::mt19937 rng((std::random_device())());
    return rng();
}

static inline void write_be64(std::vector<unsigned char>& buf, size_t off, uint64_t v) {
    for (int i = 7; i >= 0; --i) buf[off + (7 - i)] = static_cast<unsigned char>((v >> (i*8)) & 0xFF);
}
static inline void write_be32(std::vector<unsigned char>& buf, size_t off, uint32_t v) {
    for (int i = 3; i >= 0; --i) buf[off + (3 - i)] = static_cast<unsigned char>((v >> (i*8)) & 0xFF);
}
static inline void write_be16(std::vector<unsigned char>& buf, size_t off, uint16_t v) {
    buf[off]     = static_cast<unsigned char>((v >> 8) & 0xFF);
    buf[off + 1] = static_cast<unsigned char>(v & 0xFF);
}

class UdpTracker : public BaseTracker {
public:
    UdpTracker(const std::string& url) : BaseTracker(url) {}

    TrackerResponse announce(const std::array<uint8_t, 20>& infoHash, const std::string& peerId, const std::atomic<size_t>& uploaded, const std::atomic<size_t>& downloaded, const std::atomic<size_t>& total) override;

    std::string protocol() const override { return "udp"; }

private:
    // helper to perform one send/recv with tIimeout and return received bytes or empty
    static std::vector<unsigned char> do_exchange(boost::asio::io_context& io,
                                                  udp::socket& socket,
                                                  const udp::endpoint& ep,
                                                  const std::vector<unsigned char>& out_buf,
                                                  size_t min_resp_len,
                                                  std::chrono::seconds timeout);
                                                  
    // BE helpers to read integers from network-order buffer
    static inline uint64_t read_be64(const unsigned char* p) {
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) v = (v << 8) | p[i];
        return v;
    }
    static inline uint32_t read_be32(const unsigned char* p) {
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v = (v << 8) | p[i];
        return v;
    }
};
