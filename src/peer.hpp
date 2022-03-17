#ifndef __PEER_HPP__
#define __PEER_HPP__

#include <jthread.hpp>
#include <cstring>
#include "networking_include_everywhere.hpp"

class Peer {
	// The socket we are listening and sending on
	zt::Socket socket;
	// Listening thread
	std::jthread listeningThread;

	// Buffer we receive data in
	std::byte buffer[30];

public:
	Peer() {}
	Peer(zt::Socket&& _socket, bool stayPaused = false) : socket(std::move(_socket)),
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

	// Send some data to to the connected peer
	void send(const void* data, size_t size) const { reference_cast<zt::Socket>(socket).send(data, size); }

protected:
	void threadFunction(std::stop_token stop){
		// Loop unil the thread is requested to stop
		while(!stop.stop_requested()){
			try {
				// std::cout << "threadfn:" << socket._impl.get() << std::endl;

				// Wait upto 100ms for data
				auto pollres = socket.pollEvents(zt::PollEventBitmask::ReadyToReceiveAny, 100ms);
				ZTCPP_THROW_ON_ERROR(pollres, ZTError);
		
				// If there is data ready to be received...
				if((*pollres & zt::PollEventBitmask::ReadyToReceiveAny) != 0) {
					// Clear the receive buffer // TODO: Needed?
					std::memset(buffer, 0, sizeof(buffer));

					// Receive the data
					zt::IpAddress remoteIP;
					std::uint16_t remotePort;
					auto res = socket.receiveFrom(buffer, sizeof(buffer), remoteIP, remotePort);
					ZTCPP_THROW_ON_ERROR(res, ZTError);

					// And print it out
					std::cout << "Received " << *res << " bytes from " << remoteIP << ":" << remotePort << ";\n"
								<< "		Message: " << (char*) buffer << std::endl;
				}
			} catch(ZTError e) {
				// if(std::string(e.what()).find("zts_errno=107") != std::string::npos) {
				// 	// We have been disconnected and this peer is no longer valid
				// 	std::cout << "DISCONNECT" << std::endl;
				// 	return;
				// }
					
				// TODO: This is where we would need to handle a loss of connection
				std::cerr << "[ZT][Error] " << e.what() << std::endl;
			}
		}
	}
};

#endif // __PEER_HPP__