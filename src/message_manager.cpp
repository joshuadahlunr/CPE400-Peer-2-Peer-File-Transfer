#include "peer_manager.hpp"
#include "message_manager.hpp"

#include <fstream>

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
void MessageManager::processCreateFileMessage(const FileContentMessage& m){

}

// Function that processes an initial file sync
void MessageManager::processInitialFileSyncMessage(const FileInitialSyncMessage& m){
	std::cout << m.index << " / " << m.total << std::endl;
	auto time_t = to_time_t(m.timestamp);

	// Create intermediate directories
	auto folder = m.targetFile;
	create_directories(folder.remove_filename());
	// Write the content of the file to disk
	std::ofstream fout(m.targetFile);
	fout << m.fileContent;
	fout.close();

	// Copy the file into the .wnts folder
	auto wnts = wntsPath(m.targetFile);
	auto wntsFolder = wnts;
	create_directories(wntsFolder.remove_filename());
	copy(m.targetFile, wnts, std::filesystem::copy_options::update_existing);

	std::cout << m.targetFile << " - " << std::ctime(&time_t) << std::endl;
}

// Function that processes an initial file sync request
void MessageManager::processInitialFileSyncRequestMessage(const Message& m) {
	// TODO: Can we parallelize this somehow?
	// Send the content of every managed file to the newly connected node
	auto paths = enumerateAllFiles(*folders);
	for(size_t i = 0, size = paths.size(); i < size; i++){
		FileInitialSyncMessage sync;
		sync.type = Message::Type::initialSync;
		sync.targetFile = paths[i];
		sync.timestamp = std::chrono::system_clock::now();
		sync.index = i;
		sync.total = size - 1;

		std::cout << sync.index << " / " << sync.total << std::endl;
		
		std::ifstream fin(sync.targetFile);
		std::getline(fin, sync.fileContent, '\0');
		fin.close();

		PeerManager::singleton().send(sync, m.originatorNode);
	}
}

// Function that processes a file change
void MessageManager::processChangeFileMessage(const FileChangeMessage& m){

}

// Function that processes connection info from the gateway peer
void MessageManager::processConnectMessage(const ConnectMessage& m){
	// Save the backup IP addresses
	PeerManager::singleton().backupPeers = std::move(m.backupPeers);
	// Save the list of folders the network is managing
	*folders = std::move(m.managedPaths);

	// Delete managed data in preparation for data syncs
	auto paths = enumerateAllFiles(*folders);
	for(auto& path: paths) {
		auto wnts = wntsPath(path);
		remove(path);
		remove(wnts);
	}
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
