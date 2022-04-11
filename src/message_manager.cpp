#include "peer_manager.hpp"
#include "message_manager.hpp"


// Function that processes the next message currently in the message queue
//	(or waits 1/10 of a second if there is nothing in the queue)
void MessageManager::processNextMessage(){
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
		PeerManager::singleton().backupPeers = std::move(m.backupPeers);

		// TODO: delete managed data and prepare for data syncs
	}
	break; case Message::Type::disconnect:{
		auto& m = reference_cast<Message>(*msgPtr);
		std::cout << "[" << m.originatorNode << "] disconnect message" << std::endl;

		// Remove the disconnected Peer as a backup Peer
		auto& backupPeers = PeerManager::singleton().backupPeers;
		for(size_t peer = 0; peer < backupPeers.size(); peer++) {
			auto& [backupIP, _] = backupPeers[peer];
			if(backupIP == m.originatorNode)
				backupPeers.erase(backupPeers.begin() + peer);
		}

		// TODO: free any locks held by the peer
	}
	break; case Message::Type::linkLost:{
		auto& m = reference_cast<Message>(*msgPtr);
		std::cout << "[" << m.originatorNode << "] link-lost message" << std::endl;

		zt::IpAddress removedIP;
		{
			auto peerLock = PeerManager::singleton().getPeers().write_lock();
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
				if(removedIP == PeerManager::singleton().gatewayIP) {
					PeerManager::singleton().setGatewayIP(zt::IpAddress::ipv6Unspecified()); // Mark that we don't have a gateway
					auto& backupPeers = PeerManager::singleton().backupPeers;
					for(size_t peer = 0; peer < backupPeers.size(); peer++) {
						auto& [backupIP, backupPort] = backupPeers[peer];
						if(backupIP.isValid()) {
							try {
								peerLock->insert(peerLock->begin(), std::move(Peer::connect(backupIP, backupPort)));
								PeerManager::singleton().setGatewayIP(backupIP); // Mark the backup IP as our "gateway" to the rest of the network
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

		// Notify the rest of the network that a Peer disconnected
		if(removedIP.isValid()) {
			Message m;
			m.type = Message::Type::disconnect;
			m.originatorNode = removedIP;
			PeerManager::singleton().send(m); // The write lock has to be released before we send
		}
	}
	break; default:
		throw std::runtime_error("Unrecognized message type");
	}
}