#ifndef __PEER_MANAGER_HPP__
#define __PEER_MANAGER_HPP__

#include "peer.hpp"
#include "monitor.hpp"
#include "ztnode.hpp"
#include "message_manager.hpp"

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
	// The IP address of the Peer which provides connectivity to the rest of the network
	zt::IpAddress gatewayIP = zt::IpAddress::ipv6Unspecified();
	// List of IP address we can replace the gatewayIP with should the gatewayIP go offline
	std::vector<std::pair<zt::IpAddress, uint16_t>> backupPeers;

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

					// Determine the other IP addresses the new Peer should connect to if we go down
					std::vector<std::pair<zt::IpAddress, uint16_t>> backupPeers;
					zt::IpAddress peerIP;
					{
						auto peerLock = peers.write_lock();
						for(auto& peer: *peerLock)
							backupPeers.emplace_back(peer.getRemoteIP(), peer.getRemotePort());

						// Add the peer to the peer list
						peerLock->emplace_back(std::move(*sock));
						peerIP = peerLock->back().getRemoteIP();
					}

					// Notify the new peer of its backup Peers
					ConnectMessage connectMessage;
					connectMessage.type = Message::Type::connect;
					connectMessage.backupPeers = backupPeers;
					send(connectMessage, peerIP); // The write lock must be released before we send, otherwise we have the same thread taking multiple locks

					// Add a message to the queue requesting all of the data be sent to the new node
					auto syncRequest = std::make_unique<Message>();
					syncRequest->type = Message::Type::initialSyncRequest;
					syncRequest->originatorNode = peerIP;
					MessageManager::singleton().messageQueue->emplace(2, std::move(syncRequest)); // Same priority as disconnect

					std::cout << "Accepted Connection from: " << peerIP << std::endl;
				}
			}

			// Close the socket when we stop the thread
			connectionSocket.close();
		});
	}

	// Function which sends a payload message to the specified <destination>
	// NOTE: By default the address is unspecififed which is taken to mean everyone
	template<typename MSG>
	void send(MSG msg, zt::IpAddress destination = zt::IpAddress::ipv6Unspecified(), bool broadcastToSelf = true) const {
		// Add routing information to the message
		msg.receiverNode = destination;
		msg.senderNode = ZeroTierNode::singleton().getIP();
		if(msg.originatorNode == zt::IpAddress::ipv6Unspecified()) msg.originatorNode = msg.senderNode;
		msg.messageHash = msg.hash();

		// Serialize the data
		std::stringstream stream;
		boost::archive::binary_oarchive ar(stream, archiveFlags);
		ar << msg;

		// Forward the data (based on the added routing information)
		routeData(nonstd::span<std::byte>{(std::byte*) stream.str().data(), stream.str().size()}, destination,
			broadcastToSelf ? zt::IpAddress::ipv6Unspecified() : zt::IpAddress::ipv6Loopback());

		// Move the message into the buffer of old messages
		MessageManager::singleton().oldMessages.emplace_back(std::make_unique<MSG>(std::move(msg)));
	}


	// Function which gets a reference to the array of peers
	monitor<std::vector<Peer>>& getPeers() { return peers; }

	// Functions which get or set the gateway IP
	const zt::IpAddress& getGatewayIP() { return gatewayIP; }
	void setGatewayIP(const zt::IpAddress& ip) { gatewayIP = ip; }

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
				MessageManager::singleton().deserializeMessage(data);
		};


		// If the destination is unspecified forward the data to everyone
		if(destination == zt::IpAddress::ipv6Unspecified() || destination == zt::IpAddress::ipv4Unspecified())
			forward2all();
		// If we are the destination, process the data locally
		else if(destination == zt::IpAddress::ipv6Loopback() || destination == zt::IpAddress::ipv4Loopback() || destination == ZeroTierNode::singleton().getIP())
			MessageManager::singleton().deserializeMessage(data);
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
};

#endif // __PEER_MANAGER_HPP__