#ifndef __PEER_HPP__
#define __PEER_HPP__

#include <jthread.hpp>
#include <cstring>
#include "networking_include_everywhere.hpp"

// Class representing a connection to another Peer on the network, it wraps a TCP socket and listening thread
class Peer {
	// The socket we are listening and sending on
	zt::Socket socket;
	// Listening thread
	std::jthread listeningThread;

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

	// Send some data to to the connected peer
	void send(const void* data, uint64_t size) const {
		zt::Socket& socket = reference_cast<zt::Socket>(this->socket);
		// Before we send data, we send the size of the data
		socket.send(&size, sizeof(size));
		socket.send(data, size); 
	}

protected:
	void threadFunction(std::stop_token stop){
		// Variable tracking how much data we should expect to receive in this message
		uint64_t dataSize = 0;
		// Variable tracking how much data we have currently received
		uint64_t dataReceived = 0;

		// Loop unil the thread is requested to stop
		while(!stop.stop_requested()){
			try {
				// std::cout << "threadfn:" << socket._impl.get() << std::endl;

				// Wait upto 100ms for data
				auto pollres = socket.pollEvents(zt::PollEventBitmask::ReadyToReceiveAny, 100ms);
				ZTCPP_THROW_ON_ERROR(pollres, ZTError);
		
				// If there is data ready to be received...
				if((*pollres & zt::PollEventBitmask::ReadyToReceiveAny) != 0) {
					// Get a pointer to the buffer's memory
					std::byte* bufferMem = &buffer[0];

					// If we haven't determined how much data we have to receive...
					if(dataSize == 0){
						// Read a uint64 worth of data (remember it may take multiple loop iterations to receive that data)
						auto res = socket.receive(bufferMem + dataReceived, sizeof(dataSize) - dataReceived);
						ZTCPP_THROW_ON_ERROR(res, ZTError);
						dataReceived += *res;

						// Once we have read the uint64...
						if(dataReceived >= sizeof(dataSize)){
							// Mark it is as our data size
							dataSize = *((uint64_t*) bufferMem);
							dataReceived -= sizeof(dataSize); // Subtracting to account for the possibility of extra data

							// If there is extra data in the buffer, move it to the front and resize the buffer to match our data
							if(dataReceived > 0) memmove(bufferMem, bufferMem + sizeof(dataSize), buffer.size() - sizeof(dataSize)); // TODO: Needed?
							buffer.resize(dataSize); // TODO: should this only be allowed to grow the buffer?
						}
					// If we have determined how much data we have to receive...
					} else {
						// Read <dataSize> bytes of data (remember it may take multiple loop iterations to receive that data)
						auto res = socket.receive(bufferMem + dataReceived, buffer.size() - dataReceived);
						ZTCPP_THROW_ON_ERROR(res, ZTError);
						dataReceived += *res;

						// Once we have read <dataSize> bytes...
						if(dataReceived >= dataSize) {
							// Process the message
							processMessage(dataSize);

							// If there is extra data in the buffer, move it to the front
							if(dataReceived > 0) memmove(bufferMem, bufferMem + dataSize, buffer.size() - dataSize); // TODO: Needed?

							// Reset our data size back to 0 (we need to get the size of the next message from the network)
							dataReceived -= dataSize; // Subtracting to account for the possibility of extra data
							dataSize = 0;
						}
					}
				}
			} catch(ZTError e) {
				std::string error = e.what();
				if(error.find("zts_errno=107") != std::string::npos
					| error.find("zts_poll returned ZTS_POLLERR") != std::string::npos)
				{
					// We have been disconnected and this peer is no longer valid
					std::cout << "DISCONNECT" << std::endl;
					// TODO: Remove from list of peers
					return;
				}
					
				// TODO: This is where we would need to handle a loss of connection
				std::cerr << "[ZT][Error] " << error << std::endl;
			}
		}
	}

	// Function that processes a message
	void processMessage(size_t dataSize) {
		std::cout << "Received " << dataSize << " bytes:\n" << (char*) buffer.data() << std::endl;
	}
};

#endif // __PEER_HPP__