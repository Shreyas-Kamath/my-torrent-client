#pragma once

#include <string>
#include <vector>

#include <Utils.hpp>
#include <Peer.hpp>
#include <Bencode.hpp>

class BaseTracker {
public:
    BaseTracker(const std::string& url): trackerUrl(url) {}
    virtual ~BaseTracker() = default;

    virtual std::vector<Peer> announce(const std::array<uint8_t, 20>& infoHash, const std::string& peerId) = 0;

    virtual std::string protocol() const = 0;

protected:
    std::string trackerUrl;
};
