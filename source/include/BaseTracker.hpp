#pragma once

#include <string>
#include <vector>

#include <Utils.hpp>

class BaseTracker {
public:
    BaseTracker(const std::string& url): trackerUrl(url) {}
    virtual ~BaseTracker() = default;

    virtual std::string announce(const std::string& infoHash,
                                              const std::string& peerId) = 0;

    virtual std::string protocol() const = 0;

protected:
    std::string trackerUrl;
};
