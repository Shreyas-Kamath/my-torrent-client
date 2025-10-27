#pragma once

#include <boost/asio.hpp>
#include <unordered_set>
#include <vector>
#include <memory>
#include <atomic>
#include <csignal>
#include <functional>
#include <iostream>

#include <Utils.hpp>
#include <Bencode.hpp>
#include <TorrentFile.hpp>
#include <TrackerFactory.hpp>
#include <Peer.hpp>
#include <PeerConnection.hpp>

class TorrentClient {
public:
    TorrentClient(const std::string& torrent_file_path)
        : io_(),
          acceptor_(io_, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 31616))
    {
        auto in = read_from_file(torrent_file_path);
        metadata_ = parse_torrent(in);

        stats_ = std::make_unique<Stats>();
        pm_ = std::make_unique<PieceManager>(
            metadata_.total_size,
            metadata_.piece_hashes.size(),
            metadata_.piece_length,
            metadata_.piece_hashes,
            metadata_.name,
            *stats_
        );
        pm_->init_files(metadata_.files);

        // initialize trackers
        for (const auto& tier : metadata_.announce_list)
            for (const auto& url : tier)
                trackers_.push_back(make_tracker(url));

        // set static pointer for signal handler
        instance_ = this;
        std::signal(SIGINT, &TorrentClient::signal_handler);
    }

    ~TorrentClient() = default;

    void run() {
        // setup timers
        announce_timer_ = std::make_shared<boost::asio::steady_timer>(io_);
        stats_timer_ = std::make_shared<boost::asio::steady_timer>(io_, std::chrono::seconds(1));

        // start first announce and stats
        announce_fn();
        stats_fn();
        start_accept();
        
        // event loop
        while (!stop_signal_.load()) io_.run_one();

        shutdown();
    }

private:
    // static signal handler
    static void signal_handler(int) {
        if (instance_) instance_->stop_signal_.store(true);
    }

    void start_accept() {
        auto socket = std::make_shared<tcp::socket>(io_);
        
        acceptor_.async_accept(*socket, 
            [this, socket](boost::system::error_code ec) {
            if (!ec) { handle_incoming_connection(socket); std::print("acceptor got a connection\n"); }
            start_accept();
        });
    }

    void handle_incoming_connection(std::shared_ptr<tcp::socket> socket) {
        auto conn = std::make_shared<PeerConnection>(std::move(*socket), metadata_.info_hash, "-CT0001-123456789012", *pm_);

        connections_.push_back(conn);
        conn->start_inbound();
    }

    void announce_fn() {
        // kick stale peers off the pool
        connections_.erase(
            std::remove_if(connections_.begin(), connections_.end(), [](const auto& conn) {
                return (conn == nullptr || !conn->is_alive());
            }),
            connections_.end()
        );

        for (auto& tracker : trackers_) {
            try {
                auto response = tracker->announce(metadata_.info_hash,
                                                  "-CT0001-123456789012",
                                                  stats_->uploaded_bytes,
                                                  stats_->downloaded_bytes,
                                                  stats_->total_size);

                for (auto& peer : response.peers) {
                    auto exists = std::any_of(connections_.begin(), connections_.end(), [&peer](const auto& conn) {
                        return conn && conn->peer() == peer;
                    });

                    if (!exists) {
                        auto conn = std::make_shared<PeerConnection>(
                            io_, peer, metadata_.info_hash, "-CT0001-123456789012", *pm_
                        );
                        connections_.push_back(conn);
                        conn->start();
                    }
                }

            } catch (const std::exception& e) {
                std::cerr << "Tracker " << tracker->name() << " failed: " << e.what() << "\n";
            }
        }

        // schedule next announce
        announce_timer_->expires_after(std::chrono::seconds(180));
        announce_timer_->async_wait([this](const boost::system::error_code& ec) {
            if (!ec) announce_fn();
        });
    }

    void stats_fn() {
        stats_->display();
        stats_timer_->expires_after(std::chrono::seconds(1));
        stats_timer_->async_wait([this](const boost::system::error_code& ec) {
            if (!ec) stats_fn();
        });
    }

    void shutdown() {
        std::cout << "\nShutting down...\n";
        announce_timer_->cancel();
        stats_timer_->cancel();
        io_.stop();
        for (auto& conn : connections_) conn->stop();
    }

private:
    boost::asio::io_context io_;
    boost::asio::ip::tcp::acceptor acceptor_;

    Metadata metadata_;
    std::unique_ptr<Stats> stats_;
    std::unique_ptr<PieceManager> pm_;

    std::vector<std::shared_ptr<BaseTracker>> trackers_;
    std::vector<std::shared_ptr<PeerConnection>> connections_;

    std::shared_ptr<boost::asio::steady_timer> announce_timer_;
    std::shared_ptr<boost::asio::steady_timer> stats_timer_;

    std::atomic<bool> stop_signal_{false};
    static TorrentClient* instance_;
};

// static instance pointer
TorrentClient* TorrentClient::instance_ = nullptr;
