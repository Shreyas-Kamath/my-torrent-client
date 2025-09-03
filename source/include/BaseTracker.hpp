#pragma once

#include <string>
#include <vector>

class BaseTracker {
public:
    BaseTracker(const std::string& url): trackerUrl(url) {}
    virtual ~BaseTracker() = default;

    virtual std::vector<std::string> announce(const std::string& infoHash,
                                              const std::string& peerId) = 0;

    virtual std::string protocol() const = 0;

protected:
    std::string trackerUrl;
};
