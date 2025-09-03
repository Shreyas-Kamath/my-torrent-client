// parse a .torrent file and extract its metadata

#pragma once

#include <vector>
#include <string>
#include <optional>
#include <array>
#include <print>
#include <ranges>
#include <cstring>
#include <openssl/sha.h>

// my headers
#include <Bencode.hpp>

struct TorrentFile {
	std::string path;
	uint64_t length;
};

struct Metadata {
	std::string announce;							     // primary tracker URL (for older torrents)
	std::vector<std::vector<std::string>> announce_list; // list of tracker URLs by hierarchy

	std::string name;																	// ------
	uint64_t piece_length;																//      | <-- contained in
	std::vector<std::array<uint8_t, 20>> piece_hashes;  // each hash is 20 bytes		//      | <-- info dict
	std::vector<TorrentFile> files;						// for multi-file torrents		// ------

	std::array<uint8_t, 20> info_hash;					// SHA1 hash of the info dictionary

	// OPTIONALS

	std::optional<std::string> comment;
	std::optional<std::string> created_by;
	std::optional<uint64_t> creation_date;				// epoch time

	// -- END OPTIONALS

	uint64_t total_size{};
};

Metadata parse_torrent(const std::string&);