#include <Utils.hpp>

std::string read_from_file(const std::string& path) {
    std::string input;
	std::ifstream file(path, std::ios::binary | std::ios::ate);

	if (!file.is_open()) {
		throw std::runtime_error("Could not open file: " + path);
	}

	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);

	std::string data(size, '\0');
	file.read(data.data(), size);

    return data;
}

ParsedUrl parse_url(const std::string& url) {
	ParsedUrl result;

    std::string tmp = url;

    // strip scheme
    auto pos = tmp.find("://");
    if (pos != std::string::npos) {
        tmp = tmp.substr(pos + 3);
    }

    // split host[:port] and path
    auto slash = tmp.find('/');
    std::string hostport = (slash != std::string::npos) ? tmp.substr(0, slash) : tmp;
    result.target = (slash != std::string::npos) ? tmp.substr(slash) : "/";

    // split host and port
    auto colon = hostport.find(':');
    if (colon != std::string::npos) {
        result.host = hostport.substr(0, colon);
        result.port = hostport.substr(colon + 1);
    } else {
        result.host = hostport;
    }

    return result;
}
