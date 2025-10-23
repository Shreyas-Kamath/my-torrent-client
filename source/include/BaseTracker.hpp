#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <optional>

#include <Utils.hpp>
#include <Peer.hpp>
#include <Bencode.hpp>

struct TrackerResponse {
    std::vector<Peer> peers;
    std::optional<uint32_t> interval;
};

class BaseTracker {
public:
    BaseTracker(const std::string& url): trackerUrl(url), parsed(parse_url(trackerUrl)) {}
    virtual ~BaseTracker() = default;

    virtual TrackerResponse announce(const std::array<uint8_t, 20>& infoHash, const std::string& peerId, const std::atomic<size_t>& uploaded, const std::atomic<size_t>& downloaded, const std::atomic<size_t>& total) = 0;

    virtual std::string protocol() const = 0;

    const std::string& name() const { return trackerUrl; }

protected:
    std::string trackerUrl{};
    ParsedUrl parsed;
};
