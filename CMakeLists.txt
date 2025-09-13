cmake_minimum_required(VERSION 3.10)
project(my_torrent_client)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)
find_package(Boost COMPONENTS beast asio REQUIRED)

add_executable(
    ctorrent
    main.cpp
    source/src/Utils.cpp
    source/src/TorrentFile.cpp
    source/src/Bencode.cpp
    source/src/HttpTracker.cpp
    source/src/HttpsTracker.cpp
    source/src/Peer.cpp
    source/src/PeerConnection.cpp
    source/src/PieceManager.cpp
)

target_include_directories(
    ctorrent PRIVATE
    source/include
    ${OPENSSL_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(
    ctorrent PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    Boost::asio
    Boost::beast
)

