#include <memory>
#include <string>
#include <stdexcept>

#include <BaseTracker.hpp>
#include <HttpTracker.hpp>
#include <HttpsTracker.hpp>
// #include <UdpTracker.hpp>

std::unique_ptr<BaseTracker> make_tracker(const std::string& url) {
    if (url.rfind("http://", 0) == 0) return std::make_unique<HttpTracker>(url);
    else if (url.rfind("https://", 0) == 0) return std::make_unique<HttpsTracker>(url);
    // else if (url.rfind("udp://", 0) == 0) return std::make_unique<UdpTracker>(url);
    throw std::invalid_argument("Unsupported tracker URL: " + url);
}
