#include <TorrentFile.hpp>

Metadata parse_torrent(const std::string& in) {

	BEncodeParser parser(in);

	auto dict = parser.parse().as_dict();

    Metadata meta{};

    // Required: announce
    auto it = dict.find("announce");
    if (it != dict.end() && it->second.is_string()) {
        meta.announce = it->second.as_string();
		std::print("Found announce URL: {}\n", meta.announce);
    }

    // Optional: announce-list
    int tiers{ 1 };

    it = dict.find("announce-list");
    if (it != dict.end() && it->second.is_list()) {
        for (const auto& tier_val : it->second.as_list()) {
            std::vector<std::string> tier;
            if (tier_val.is_list()) {
                for (const auto& tracker : tier_val.as_list()) {
                    if (tracker.is_string()) tier.push_back(tracker.as_string());
					std::print("Tier {}, found URL {}\n", tiers, tracker.as_string());
                }
            }
            ++tiers;
            if (!tier.empty()) meta.announce_list.push_back(std::move(tier));
        }
    }

    // Info dictionary (required)
    it = dict.find("info");
    if (it == dict.end() || !it->second.is_dict())
        throw std::runtime_error("Missing info dictionary");
    const auto& info = it->second.as_dict();

    // Name
    auto name_it = info.find("name");
    if (name_it != info.end() && name_it->second.is_string()) {
        meta.name = name_it->second.as_string();
		std::print("Found torrent name: {}\n", meta.name);
    }
       

    // Piece length
    auto pl_it = info.find("piece length");
    if (pl_it != info.end() && pl_it->second.is_int()) {
        meta.piece_length = static_cast<uint64_t>(pl_it->second.as_int());
		std::print("Piece length: {}\n", meta.piece_length);
    }
        

    // Pieces (concatenated SHA1 hashes)
    auto pieces_it = info.find("pieces");
    if (pieces_it != info.end() && pieces_it->second.is_string()) {
        const std::string& pieces_str = pieces_it->second.as_string();
        size_t n = pieces_str.size() / 20;
        meta.piece_hashes.resize(n);
        for (size_t i = 0; i < n; ++i) {
            std::memcpy(meta.piece_hashes[i].data(), pieces_str.data() + i * 20, 20);
        }
    }

    // Files
    auto files_it = info.find("files");
    if (files_it != info.end() && files_it->second.is_list()) {
        // Multi-file torrent

        for (const auto& fval : files_it->second.as_list()) {
            if (!fval.is_dict()) continue;
            const auto& fdict = fval.as_dict();
            TorrentFile file;

            // Path (list of strings)
            auto path_it = fdict.find("path");
            if (path_it != fdict.end() && path_it->second.is_list()) {
                auto full_path = path_it->second.as_list()
                    | std::views::transform([](const BEncodeValue& b) { return b.as_string(); })
                    | std::views::join_with('/')
					| std::ranges::to<std::string>();

				std::print("Adding file: {}\n", full_path);
                file.path = std::move(full_path);
            }

            // Length
            auto len_it = fdict.find("length");
            if (len_it != fdict.end() && len_it->second.is_int()) {
                file.length = static_cast<uint64_t>(len_it->second.as_int());
                meta.total_size += file.length;
            }

            meta.files.push_back(std::move(file));
        }
    }
    else {
        // Single-file torrent
        auto len_it = info.find("length");
        if (len_it != info.end() && len_it->second.is_int()) {
            meta.files.push_back({ meta.name, static_cast<uint64_t>(len_it->second.as_int()) });
            meta.total_size += len_it->second.as_int();
        }
    }

    // Optionals
    auto comment_it = dict.find("comment");
    if (comment_it != dict.end() && comment_it->second.is_string())
        meta.comment = comment_it->second.as_string();

    auto created_by_it = dict.find("created by");
    if (created_by_it != dict.end() && created_by_it->second.is_string())
    {
        meta.created_by = created_by_it->second.as_string();
        std::print("Created by: {}\n", meta.created_by.value());
    }

    auto creation_date_it = dict.find("creation date");
    if (creation_date_it != dict.end() && creation_date_it->second.is_int())
        meta.creation_date = static_cast<uint64_t>(creation_date_it->second.as_int());

    // Info hash (SHA1 of bencoded info dictionary)

	const auto& [start, end] = parser.get_info_start_end();

	std::string info_bencoded = in.substr(start, end - start);

    SHA1(reinterpret_cast<const unsigned char*>(info_bencoded.data()), info_bencoded.size(), meta.info_hash.data());

    return meta;
}