#ifndef __PEER_MANAGER_HPP__
#define __PEER_MANAGER_HPP__

#include "peer.hpp"
#include "monitor.hpp"
#include "ztnode.hpp"

#include <lockfree_skiplist_priority_queue.h>
#include "messages.hpp"

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

	// Queue of messages waiting to be processed (It is a non-blocking [skiplist based] concurrent queue)
	// NOTE: Lower priorities = faster execution
	mutable skipListQueue<std::unique_ptr<Message>> messageQueue;

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
					ConnectMessage m;
					m.type = Message::Type::connect;
					m.backupPeers = backupPeers;
					send(m, peerIP); // The write lock must be released before we send, otherwise we have the same thread taking multiple locks


					std::cout << "Accepted Connection from:\n" << peerIP << std::endl;
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
		boost::archive::binary_oarchive ar(stream, archiveFlags);
		ar << msg;

		// Forward the data (based on the added routing information)
		routeData(nonstd::span<std::byte>{(std::byte*) stream.str().data(), stream.str().size()}, destination,
			broadcastToSelf ? zt::IpAddress::ipv6Unspecified() : zt::IpAddress::ipv6Loopback());
	}

	// Function that processes the next message currently in the message queue
	//	(or waits 1/10 of a second if there is nothing in the queue)
	void processNextMessage(){
		auto min = messageQueue.findMin();
		// If the queue is empty, sleep for 100ms
		if(min == nullptr) {
			std::this_thread::sleep_for(100ms);
			return;
		}

		// Save the message and remove the node from the queue
		std::unique_ptr<Message> msgPtr = std::move(min->value);
		messageQueue.removeMin();


		// Deserialize the message as the same type of message that was delivered
		switch(msgPtr->type) {
		break; case Message::Type::payload:{
			auto& m = reference_cast<PayloadMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "][payload]:\n" << m.payload << std::endl;
		}
		break; case Message::Type::lock:{
			auto& m = reference_cast<FileMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] lock message" << std::endl;
			// TODO: Process lock
		}
		break; case Message::Type::unlock:{
			auto& m = reference_cast<FileMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] unlock message" << std::endl;
			// TODO: Process unlock
		}
		break; case Message::Type::deleteFile:{
			auto& m = reference_cast<FileMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] delete message" << std::endl;
			// TODO: Process delete
		}
		break; case Message::Type::create:{
			auto& m = reference_cast<FileCreateMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] create message" << std::endl;
			// TODO: Process create
		}
		break; case Message::Type::change:{
			auto& m = reference_cast<FileChangeMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] change message" << std::endl;
			// TODO: Process change
		}
		break; case Message::Type::connect:{
			auto& m = reference_cast<ConnectMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] connect message" << std::endl;

			// Save the backup IP addresses
			backupPeers = std::move(m.backupPeers);

			// TODO: delete managed data and prepare for data syncs
		}
		break; case Message::Type::disconnect:{
			auto& m = reference_cast<DisconnectConnectMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] disconnect message" << std::endl;
			// TODO: Process disconnect
		}
		break; case Message::Type::linkLost:{
			auto& m = reference_cast<Message>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] link-lost message" << std::endl;
			
			zt::IpAddress removedIP;
			{
				auto peerLock = peers.write_lock();
				// Find the Peer that disconnected
				size_t index = -1;
				for(size_t i = 0; i < peerLock->size(); i++)
					if(peerLock[i].getRemoteIP() == m.originatorNode) {
						index = i;
						break;
					}
				if(index != std::numeric_limits<size_t>::max()){
					// Remove the disconnected Peer from our list of Peers
					removedIP = peerLock[index].getRemoteIP();
					peerLock->erase(peerLock->begin() + index);

					// If the removed Peer was our gateway, connect to one of the backup Peers so that the nextwork doesn't become segmented
					if(removedIP == gatewayIP) {
						setGatewayIP(zt::IpAddress::ipv6Unspecified()); // Mark that we don't have a gateway
						for(size_t peer = 0; peer < backupPeers.size(); peer++) {
							auto& [backupIP, backupPort] = backupPeers[peer];
							if(backupIP.isValid()) {
								try {
									peerLock->insert(peerLock->begin(), std::move(Peer::connect(backupIP, backupPort)));
									setGatewayIP(backupIP); // Mark the backup IP as our "gateway" to the rest of the network
									std::cout << "Updated gateway to: " << backupIP << std::endl;

									// If a backup Peer becomes our gateway remove it as a backup and stop looking
									backupPeers.erase(backupPeers.begin() + peer);
									break;
								// If we failed to connect to a Peer, try the next one
								} catch (std::runtime_error) { continue; }
							}
						}
					}
				}
			}

			// TODO: Notify the rest of the network that a peer disconnected
			}
		}
		break; default:
			throw std::runtime_error("Unrecognized message type");
		}
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


	// Function that deserializes a message received from the network and adds it to the message queue
	void deserializeMessage(const std::span<std::byte> data) const {
		// Extract the type of message
		Message::Type type = (Message::Type) uint8_t(data[10]);
		if((uint8_t)type == 0) type = (Message::Type) uint8_t(data[5]);
		// Copy the data into a deserialization buffer
		std::stringstream backing({(char*) data.data(), data.size()});
		boost::archive::binary_iarchive ar(backing, archiveFlags);


		// Deserialize the message as the same type of message that was delivered and add it to the message queue
		switch(type) {
		break; case Message::Type::payload:{
			auto m = std::make_unique<PayloadMessage>();
			ar >> *m;
			// Payloads have a low priority
			messageQueue.insert(10, std::move(m));
		}
		break; case Message::Type::lock:{
			auto m = std::make_unique<FileMessage>();
			ar >> *m;
			// File messages have priority 5
			messageQueue.insert(5, std::move(m));
		}
		break; case Message::Type::unlock:{
			auto m = std::make_unique<FileMessage>();
			ar >> *m;
			// File messages have priority 5
			messageQueue.insert(5, std::move(m));
		}
		break; case Message::Type::deleteFile:{
			auto m = std::make_unique<FileMessage>();
			ar >> *m;
			// File messages have priority 5
			messageQueue.insert(5, std::move(m));
		}
		break; case Message::Type::create:{
			auto m = std::make_unique<FileCreateMessage>();
			ar >> *m;
			// File messages have priority 5
			messageQueue.insert(5, std::move(m));
		}
		break; case Message::Type::change:{
			auto m = std::make_unique<FileChangeMessage>();
			ar >> *m;
			// File messages have priority 5
			messageQueue.insert(5, std::move(m));
		}
		break; case Message::Type::connect:{
			auto m = std::make_unique<ConnectMessage>();
			ar >> *m;
			// Connect has highest priority
			messageQueue.insert(1, std::move(m));
		}
		break; case Message::Type::disconnect:{
			auto m = std::make_unique<DisconnectConnectMessage>();
			ar >> *m;
			// Disconnect is processed after disconnect
			messageQueue.insert(2, std::move(m));
		}
		break; default:
			throw std::runtime_error("Unrecognized message type");
		}
	}
};

#endif // __PEER_MANAGER_HPP__