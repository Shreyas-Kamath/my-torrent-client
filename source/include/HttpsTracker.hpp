#pragma once

#include <BaseTracker.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
namespace ssl   = net::ssl;
using tcp       = net::ip::tcp;

class HttpsTracker : public BaseTracker {
public:
    HttpsTracker(const std::string& url) : BaseTracker(url) {}

    std::string announce(const std::string& infoHash, const std::string& peerId) override;

    std::string protocol() const override { return "https"; }
};
