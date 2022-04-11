#ifndef __PEER_HPP__
#define __PEER_HPP__

#include <jthread.hpp>
#include <cstring>
#include "messages.hpp"

#include "networking_include_everywhere.hpp"

// Class representing a connection to another Peer on the network, it wraps a TCP socket and listening thread
class Peer {
	// The socket we are listening and sending on
	zt::Socket socket;
	// Listening thread
	std::jthread listeningThread;

	// Cached IP address and port of the remote peer
	mutable zt::IpAddress remoteIP;
	mutable uint16_t remotePort = -1;

	// Buffer we receive data in
	std::vector<std::byte> buffer = std::vector<std::byte>{30, {}};

public:
	Peer() {}
	Peer(zt::Socket&& _socket) : socket(std::move(_socket)),
		listeningThread([this](std::stop_token stop){ this->threadFunction(stop); })
	{ }

	Peer(Peer&& other) { *this = std::move(other); }
	Peer& operator=(Peer&& other) {
		// Wait for the other peer's thread to shutdown before we move
		if(other.listeningThread.joinable()) {
			other.listeningThread.request_stop();
			other.listeningThread.join();
		}

		socket = std::move(other.socket);
		listeningThread = std::jthread([this](std::stop_token stop){ this->threadFunction(stop); });

		return *this;
	};

	// Function that returns a new peer representing a connection to the provided ip and port.
	// Attempts the connection <retryAttempts> times (0 = infinite times, default 3)
	//	with a delay of <timeBetweenAttempts> (default 100ms) between each attempt.
	template<typename Duration = std::chrono::milliseconds>
	static Peer connect(const zt::IpAddress& ip, uint16_t port, size_t retryAttempts = 3, Duration timeBetweenAttempts = 100ms) {
		// We change 0 to the maximum number stored in a size_t (heat death of the unversise timeframe attempts)
		if(retryAttempts == 0) retryAttempts--;

		zt::Socket connectionSocket;
		// Attempt to open a connection <retryAttempts> times
		for(size_t i = 0; i < retryAttempts && !connectionSocket.isOpen(); i++) {
			// Pause between each attempt
			if(i) std::this_thread::sleep_for(timeBetweenAttempts);

			try {
				ZTCPP_THROW_ON_ERROR(connectionSocket.init(zt::SocketDomain::InternetProtocol_IPv6, zt::SocketType::Stream), ZTError);
				ZTCPP_THROW_ON_ERROR(connectionSocket.connect(ip, port), ZTError);
			} catch(ZTError) { /* Do nothing*/ }
		}

		// Throw an exception if we failed to connect
		if(!connectionSocket.isOpen())
			throw std::runtime_error("Failed to connect");

		return { std::move(connectionSocket) };
	}


	// Return a const reference to the managed socket
	const zt::Socket& getSocket() const { return socket; }
	// Return the IP address of this peer (with caching)
	zt::IpAddress getRemoteIP() const {
		if(!remoteIP.isValid()) {
			auto res = socket.getRemoteIpAddress();
			ZTCPP_THROW_ON_ERROR(res, ZTError);
			remoteIP = *res;
		}
		return remoteIP;
	}
	// Return the Port of this peer (with caching)
	uint16_t getRemotePort() const {
		if(remotePort == std::numeric_limits<uint16_t>::max()) {
			auto res = socket.getRemotePort();
			ZTCPP_THROW_ON_ERROR(res, ZTError);
			remotePort = *res;
		}
		return remotePort;
	}

	// Send some data to to the connected peer
	void send(const void* data, uint64_t size) const {
		zt::Socket& socket = reference_cast<zt::Socket>(this->socket);
		// Before we send data, we send the size of the data
		socket.send(&size, sizeof(size));
		socket.send(data, size);
	}

protected:
	// Function run by the Peer's thread
	void threadFunction(std::stop_token stop);

	// Function that processes a message
	void processMessage(std::span<std::byte> data);
};

#endif // __PEER_HPP__