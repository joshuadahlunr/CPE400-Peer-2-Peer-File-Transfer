#ifndef __PEER_HPP__
#define __PEER_HPP__

#include "networking_include_everywhere.hpp"
#include <jthread.hpp>
#include <cstring>
#include <optional>

class Peer {
	// The socket we are listening and sending on
	zt::Socket socket;
	// Listening thread
	std::jthread listeningThread;
	// // Whether or not we should pause the listening thread
	// bool _pause = true;

	// Buffer we recieve data in
	std::byte buffer[30];

public:
	// void pause(std::optional<bool> pause = {}) {
	// 	if(!pause.has_value()) this->_pause = !this->_pause;
	// 	else this->_pause = *pause;
	// }

	Peer() {}
	Peer(zt::Socket&& _socket, bool stayPaused = false) : socket(std::move(_socket)),
		listeningThread([this](std::stop_token stop){ this->threadFunction(stop); })
	{
		// // If our socket isn't open suspend the thread
		// while(!socket.isOpen())
		// 	std::this_thread::sleep_for(100ms);

		// pause(stayPaused);
	}

	Peer(Peer&& other) : listeningThread([this](std::stop_token stop){ this->threadFunction(stop); }) {
		// TODO: Broken
		std::cout << "Waiting for join" << std::endl;
		// Wait for the other peer's thread to shutdown before we move
		if(other.listeningThread.joinable()) {
			other.listeningThread.request_stop();
			other.listeningThread.join();
		}

		socket = std::move(other.socket);

		std::cout << "Finished constructing" << std::endl;
		// pause(other._pause);
	}
	Peer& operator=(Peer&& other) {
		// TODO: Broken

		// Wait for the other peer's thread to shutdown before we move
		if(other.listeningThread.joinable()) {
			other.listeningThread.request_stop();
			other.listeningThread.join();
		}

		socket = std::move(other.socket);
		listeningThread = std::jthread([this](std::stop_token stop){ this->threadFunction(stop); });

		// pause(other._pause);
		return *this;
	};


	// Return a const reference to the managed socket
	const zt::Socket& getSocket() const { return socket; }

	// Send some data to to the connected peer
	void send(const void* data, size_t size) const { reference_cast<zt::Socket>(socket).send(data, size); }

protected:
	void threadFunction(std::stop_token stop){
		// Loop unil the thread is requested to stop
		while(!stop.stop_requested()){
			// // Sleep while the peer is paused
			// while(_pause && !stop.stop_requested()) std::this_thread::sleep_for(10ms);
			// // If we were asked to stop while we were paused don't bother with the rest of the loop
			// if(stop.stop_requested()) break;

			std::cout << "Socket: " << socket.isOpen() << std::endl;

			try {
				// std::cout << "threadfn:" << socket._impl.get() << std::endl;

				// Wait upto 100ms for data
				auto pollres = socket.pollEvents(zt::PollEventBitmask::ReadyToReceiveAny, 100ms);
				ZTCPP_THROW_ON_ERROR(pollres, ZTError);
		
				// If there is data ready to be recieved...
				if((*pollres & zt::PollEventBitmask::ReadyToReceiveAny) != 0) {
					// Clear the recieve buffer // TODO: Needed?
					std::memset(buffer, 0, sizeof(buffer));

					// Recieve the data
					zt::IpAddress remoteIP;
					std::uint16_t remotePort;
					auto res = socket.receiveFrom(buffer, sizeof(buffer), remoteIP, remotePort);
					ZTCPP_THROW_ON_ERROR(res, ZTError);

					// And print it out
					std::cout << "Received " << *res << " bytes from " << remoteIP << ":" << remotePort << ";\n"
								<< "		Message: " << (char*) buffer << std::endl;
				}
			} catch(ZTError e) {
				// TODO: This is where we would need to handle a loss of connection
				std::cerr << "[ZT][Error] " << e.what() << std::endl;
			}
		}

		// Close the socket when we stop the thread
		socket.close();
	}
};

#endif // __PEER_HPP__