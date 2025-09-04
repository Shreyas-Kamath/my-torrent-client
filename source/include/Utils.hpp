#pragma once

#include <fstream>
#include <string>
#include <sstream>
#include <array>
#include <cstdint>
#include <iomanip>

struct ParsedUrl {
    std::string host;
    std::string port = "80"; // default
    std::string target = "/";
};

ParsedUrl parse_url(const std::string& url);

std::string read_from_file(const std::string&);

inline std::string percent_encode(const std::array<uint8_t, 20>& info_hash) {
    std::ostringstream oss;
    for (const auto& ch: info_hash) {
        oss << '%' << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
    }
    return oss.str();
}

