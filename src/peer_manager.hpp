#ifndef __PEER_MANAGER_HPP__
#define __PEER_MANAGER_HPP__

#include "peer.hpp"
#include "monitor.hpp"
#include "ztnode.hpp"

#include "networking_include_everywhere.hpp"

// Singleton class representing a list of peers, it runs a listening thread which automatically detects connecting peers
//	and adds them to its list of peers.
class PeerManager {
	// Mark Peer as a friend class
	friend class Peer;

	// Thread that listens for incoming connections
	std::jthread listeningThread;
	// List of peers (guarded by a monitor, access to this object ges through a mutex)
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

	// Function which sends a payload message to the specified <destination>
	// NOTE: By default the address is unspecififed which is taken to mean everyone
	template<typename Message>
	void send(Message msg, zt::IpAddress destination = zt::IpAddress::ipv6Unspecified(), bool broadcastToSelf = true) const {
		// Add routing information to the message
		msg.receiverNode = destination;
		msg.senderNode = msg.originatorNode = ZeroTierNode::singleton().getIP();

		// Serialize the data
		std::stringstream stream;
		boost::archive::binary_oarchive ar(stream, boost::archive::no_header);
		ar << msg;

		// Forward the data (based on the added routing information)
		routeData(nonstd::span<std::byte>{(std::byte*) stream.str().data(), stream.str().size()}, destination,
			broadcastToSelf ? zt::IpAddress::ipv6Unspecified() : zt::IpAddress::ipv6Loopback());
	}

	// Function which getts a reference to the array of peers
	monitor<std::vector<Peer>>& getPeers() { return peers; }

private:
	// Only the singleton can be constructed
	PeerManager() {}

	// Function which forwards some binary data (it figures out which nodes should receive the data)
	void routeData(const std::span<std::byte> data, const zt::IpAddress& destination, zt::IpAddress source = zt::IpAddress::ipv6Unspecified()) const {
		// Read lock the peers
		auto lock = peers.read_lock();

		// Lambda that sends the data to every connected node (including ourselves) except the node that data just came from
		auto forward2all = [&](){
			// Send the data to every peer (except the source)
			for(auto& peer: *lock)
				if(peer.getRemoteIP() != source)
					peer.send(data.data(), data.size());

			// Process the data locally (unless we are the source)
			if( !(source == zt::IpAddress::ipv6Loopback() || source == zt::IpAddress::ipv4Loopback() || source == ZeroTierNode::singleton().getIP()) )
				deserializeMessage(data);
		};


		// If the destination is unspecified forward the data to everyone
		if(destination == zt::IpAddress::ipv6Unspecified() || destination == zt::IpAddress::ipv4Unspecified())
			forward2all();
		// If we are the destination, process the data locally
		else if(destination == zt::IpAddress::ipv6Loopback() || destination == zt::IpAddress::ipv4Loopback() || destination == ZeroTierNode::singleton().getIP())
			deserializeMessage(data);
		else {
			// Find the directly connected peer we need to forward data to
			bool directLink = false;
			for(auto& peer: *lock)
				if(peer.getRemoteIP() == destination) {
					peer.send(data.data(), data.size());
					directLink = true;
					break;
				}

			// If we don't have a direct link to the destination, forward the data to everyone
			if(!directLink)
				forward2all();
		}
	}


	// Function that deserializes a message received from the network in preparation for execution
	void deserializeMessage(const std::span<std::byte> data) const {
		// Extract the type of message
		Message::Type type = (Message::Type) uint8_t(data[10]);
		// Copy the data into a deserialization buffer
		std::stringstream backing({(char*) data.data(), data.size()});
		boost::archive::binary_iarchive ar(backing, boost::archive::no_header);


		// Deserialize the message as the same type of message that was delivered
		switch(type) {
		break; case Message::Type::payload:{
			PayloadMessage m;
			ar >> m;
			std::cout << "[" << m.originatorNode << "][payload]:\n" << m.payload << std::endl;
		}
		break; case Message::Type::lock:{
			FileMessage m;
			ar >> m;
			std::cout << "lock message" << std::endl;
			// TODO: Process lock
		}
		break; case Message::Type::unlock:{
			FileMessage m;
			ar >> m;
			std::cout << "unlock message" << std::endl;
			// TODO: Process unlock
		}
		break; case Message::Type::deleteFile:{
			FileMessage m;
			ar >> m;
			std::cout << "delete message" << std::endl;
			// TODO: Process delete
		}
		break; case Message::Type::create:{
			FileCreateMessage m;
			ar >> m;
			std::cout << "create message" << std::endl;
			// TODO: Process create
		}
		break; case Message::Type::change:{
			FileChangeMessage m;
			ar >> m;
			std::cout << "change message" << std::endl;
			// TODO: Process change
		}
		break; case Message::Type::connect:{
			DisconnectConnectMessage m;
			ar >> m;
			std::cout << "connect message" << std::endl;
			// TODO: Process connect
		}
		break; case Message::Type::disconnect:{
			DisconnectConnectMessage m;
			ar >> m;
			std::cout << "disconnect message" << std::endl;
			// TODO: Process disconnect
		}
		break; default:
			throw std::runtime_error("Unrecognized message type");
		}
	}
};

#endif // __PEER_MANAGER_HPP__