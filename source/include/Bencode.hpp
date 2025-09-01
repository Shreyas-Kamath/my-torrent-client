#pragma once

#include <map>
#include <string>
#include <vector>
#include <variant>
#include <stdexcept>
#include <cstdint>

struct BEncodeValue {
	using List = std::vector<BEncodeValue>;
	using Dict = std::map<std::string, BEncodeValue>;

	std::variant<int64_t, std::string, List, Dict> value;

	bool is_int() const { return std::holds_alternative<int64_t>(value); }
	bool is_string() const { return std::holds_alternative<std::string>(value); }
	bool is_list() const { return std::holds_alternative<List>(value); }
	bool is_dict() const { return std::holds_alternative<Dict>(value); }

	int64_t as_int() const { return std::get<int64_t>(value); }
	const std::string& as_string() const { return std::get<std::string>(value); }
	const List& as_list() const { return std::get<List>(value); }
	const Dict& as_dict() const { return std::get<Dict>(value); }
};

class BEncodeParser {
public:
	explicit BEncodeParser(const std::string& input);

	BEncodeValue parse();

	std::pair<size_t, size_t> get_info_start_end() { return { _info_start, _info_end }; }

private:
	BEncodeValue parse_value();
	int64_t parse_int();
	std::string parse_string();
	BEncodeValue::List parse_list();
	BEncodeValue::Dict parse_dict();

	const std::string& _data;
	size_t pos{};

	size_t _info_start{}, _info_end{}; // positions of the "info" dictionary in the original bencoded string
};