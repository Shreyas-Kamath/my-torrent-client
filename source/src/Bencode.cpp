#include "Bencode.hpp"

BEncodeParser::BEncodeParser(const std::string& input) : _data(input), pos(0) {}

BEncodeValue BEncodeParser::parse() {
	return parse_value();
}

BEncodeValue BEncodeParser::parse_value() {
    if (pos >= _data.size()) {
        throw std::runtime_error("Unexpected end of input");
    }

    char c = _data[pos];

    if (c == 'i') {
        return BEncodeValue{ parse_int() };
    }
    else if (c == 'l') {
        ++pos;  // skip 'l'
        return BEncodeValue{ parse_list() };
    }
    else if (c == 'd') {
        ++pos;  // skip 'd'
        return BEncodeValue{ parse_dict() };
    }
    else if (isdigit(c)) {
        return BEncodeValue{ parse_string() };
    }
    else {
        throw std::runtime_error(std::string("Invalid BEncode token: ") + c);
    }
}

int64_t BEncodeParser::parse_int() {
    if (_data[pos] != 'i') throw std::runtime_error("Expected 'i' at start of integer");
    ++pos;  // skip 'i'

    size_t end = _data.find('e', pos);
    if (end == std::string::npos) throw std::runtime_error("Missing 'e' for integer");

    std::string number = _data.substr(pos, end - pos);
    pos = end + 1;  // move past 'e'

    return std::stoll(number);
}

std::string BEncodeParser::parse_string() {
    size_t colon = _data.find(':', pos);
    if (colon == std::string::npos) throw std::runtime_error("Missing ':' in string");

    std::string len_str = _data.substr(pos, colon - pos);
    size_t len = std::stoull(len_str);

    pos = colon + 1;  // skip ':'

    if (pos + len > _data.size()) throw std::runtime_error("String length exceeds input");

    std::string result = _data.substr(pos, len);
    pos += len;  // advance past the string

    return result;
}

BEncodeValue::List BEncodeParser::parse_list() {
    BEncodeValue::List list;
    while (pos < _data.size() && _data[pos] != 'e') {
        list.push_back(parse_value());
    }
    if (pos >= _data.size() || _data[pos] != 'e') throw std::runtime_error("Missing 'e' at end of list");
    ++pos;  // skip 'e'
    return list;
}

BEncodeValue::Dict BEncodeParser::parse_dict() {
    BEncodeValue::Dict dict;
    while (pos < _data.size() && _data[pos] != 'e') {
        std::string key = parse_string();
		size_t val_start = pos;
        BEncodeValue value = parse_value();
		size_t val_end = pos;

        if (key == "info") {
			_info_start = val_start;
			_info_end = val_end;
        }
        dict.emplace(std::move(key), std::move(value));
    }
    if (pos >= _data.size() || _data[pos] != 'e') throw std::runtime_error("Missing 'e' at end of dict");
    ++pos;  // skip 'e'
    return dict;
}