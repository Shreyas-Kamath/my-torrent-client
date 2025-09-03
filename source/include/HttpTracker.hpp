#pragma once

#include <BaseTracker.hpp>
#include <Utils.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>

#include <iostream>
#include <string>
#include <vector>

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
using tcp       = net::ip::tcp;

class HttpTracker : public BaseTracker {
public:
    HttpTracker(const std::string& url) : BaseTracker(url) {}

    std::vector<std::string> announce(const std::string& infoHash, const std::string& peerId) override;

    std::string protocol() const override { return "http"; }
};
