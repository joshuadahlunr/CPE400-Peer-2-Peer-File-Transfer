#ifndef __PEER_MANAGER_HPP__
#define __PEER_MANAGER_HPP__

#include "peer.hpp"
#include "monitor.hpp"

class PeerManager {
	// Thread that listens for incoming connections
	std::jthread listeningThread;
	// List of peers (guarded by a monitor, access to this object goes through a mutex)
	monitor<std::vector<Peer>> peers;

public:
	// Function which gets the PeerManager singleton
	static PeerManager& singleton() {
		static PeerManager instance;
		return instance;
	}

	// Function which starts the listening thread and sets its properties
	void setup(zt::IpAddress ip, uint16_t port, uint8_t incomingConnectionCount = 5) {
		listeningThread = std::jthread([this, ip, port, incomingConnectionCount](std::stop_token stop){
			// Create the connection socket and start listening for peers
			zt::Socket connectionSocket;
			ZTCPP_THROW_ON_ERROR(connectionSocket.init(zt::SocketDomain::InternetProtocol_IPv6, zt::SocketType::Stream), ZTError);
			ZTCPP_THROW_ON_ERROR(connectionSocket.bind(ip, port), ZTError);
			ZTCPP_THROW_ON_ERROR(connectionSocket.listen(incomingConnectionCount), ZTError);
			std::cout << "Waiting for connections..." << std::endl;
			
			// Look for a connection until the thread is requested to stop
			while(!stop.stop_requested()){
				// Wait upto 100ms for a connection
				auto pollres = connectionSocket.pollEvents(zt::PollEventBitmask::ReadyToReceiveAny, 100ms);
				ZTCPP_THROW_ON_ERROR(pollres, ZTError);
		
				// If there is a connection, accept it
				if((*pollres & zt::PollEventBitmask::ReadyToReceiveAny) != 0) {
					auto sock = connectionSocket.accept();
					ZTCPP_THROW_ON_ERROR(sock, ZTError);
					peers->emplace_back(std::move(*sock));

					std::cout << "Accepted Connection" << std::endl;
					auto ip = peers.unsafe().back().getSocket().getRemoteIpAddress();
					ZTCPP_THROW_ON_ERROR(ip, ZTError);
					std::cout << "from: " << *ip << std::endl;
				}
			}

			// Close the socket when we stop the thread
			connectionSocket.close();
		});
	}

	// Function which getts a reference to the array of peers
	monitor<std::vector<Peer>>& getPeers() { return peers; }

private:
	// Only the singleton can be constructed
	PeerManager() {}
};

#endif // __PEER_MANAGER_HPP__