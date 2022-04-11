#include "peer_manager.hpp"
#include "message_manager.hpp"

// Function that processes a file lock
void MessageManager::processLockMessage(const FileMessage& m){

}

// Function that processes a file unlock
void MessageManager::processUnlockMessage(const FileMessage& m){

}

// Function that processes a file delete
void MessageManager::processDeleteFileMessage(const FileMessage& m){

}

// Function that processes a file create
void MessageManager::processCreateFileMessage(const FileCreateMessage& m){

}

// Function that processes a file change
void MessageManager::processChangeFileMessage(const FileChangeMessage& m){

}

// Function that processes connection info from the gateway peer
void MessageManager::processConnectMessage(const ConnectMessage& m){
	// Save the backup IP addresses
	PeerManager::singleton().backupPeers = std::move(m.backupPeers);

	// TODO: delete managed data and prepare for data syncs
}

// Function that handles losing our link to a peer
void MessageManager::processLinkLostMessage(const Message& m){
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

// Function that handles a peer disconnect
void MessageManager::processDisconnectMessage(const Message& m){
	// Remove the disconnected Peer as a backup Peer
	auto& backupPeers = PeerManager::singleton().backupPeers;
	for(size_t peer = 0; peer < backupPeers.size(); peer++) {
		auto& [backupIP, _] = backupPeers[peer];
		if(backupIP == m.originatorNode)
			backupPeers.erase(backupPeers.begin() + peer);
	}

	// TODO: free any locks held by the peer
}	
