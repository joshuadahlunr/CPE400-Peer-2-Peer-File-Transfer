#include "peer_manager.hpp"
#include "message_manager.hpp"

#include <fstream>

auto lockFilePath(const std::filesystem::path& p) {
	auto lockPath = wntsPath(p);
	return lockPath.remove_filename() / (".lock." + p.filename().string());
}

// Validate the provided message, returns true if the hashes match, requests a resend and returns false otherwise
bool MessageManager::validateMessageHash(const Message& m, uint8_t offset /*= 0*/) const {
	std::cout << m.messageHash << " - " << m.hash() + offset << std::endl;
	if(m.messageHash != m.hash() + offset) {
		std::cerr << "INVALID MESSAGE" << std::endl << std::endl;
		ResendRequestMessage resend;
		resend.type = Message::Type::resendRequest;
		resend.requestedHash = m.change;
		// Request that the message be resent by its sender
		PeerManager::singleton().send(resend, m.senderNode);
		return false;
	}
	return true;
}

// Function that processes a resend request
void MessageManager::processResendRequestMessage(const ResendRequestMessage& request){
	// Find the message that needs to be resent in the old message cache, then resend it
	for(auto& m: oldMessages) {
		if(m->messageHash == request.requestedHash) {
			switch(m->type){
			break; case Message::Type::payload:				PeerManager::singleton().send(reference_cast<PayloadMessage>(*m), request.originatorNode);
			break; case Message::Type::resendRequest:		PeerManager::singleton().send(reference_cast<ResendRequestMessage>(*m), request.originatorNode);
			break; case Message::Type::lock:				PeerManager::singleton().send(reference_cast<FileMessage>(*m), request.originatorNode);
			break; case Message::Type::unlock:				PeerManager::singleton().send(reference_cast<FileMessage>(*m), request.originatorNode);
			break; case Message::Type::deleteFile:			PeerManager::singleton().send(reference_cast<FileMessage>(*m), request.originatorNode);
			break; case Message::Type::create:				PeerManager::singleton().send(reference_cast<FileContentMessage>(*m), request.originatorNode);
			break; case Message::Type::initialSync:			PeerManager::singleton().send(reference_cast<FileInitialSyncMessage>(*m), request.originatorNode);
			break; case Message::Type::initialSyncRequest:	PeerManager::singleton().send(reference_cast<Message>(*m), request.originatorNode);
			break; case Message::Type::change:				PeerManager::singleton().send(reference_cast<FileChangeMessage>(*m), request.originatorNode);
			break; case Message::Type::connect:				PeerManager::singleton().send(reference_cast<ConnectMessage>(*m), request.originatorNode);
			break; case Message::Type::disconnect:			PeerManager::singleton().send(reference_cast<Message>(*m), request.originatorNode);
			break; case Message::Type::linkLost:			PeerManager::singleton().send(reference_cast<Message>(*m), request.originatorNode);
			break; default:
				throw std::runtime_error("Unrecognized message type");
			}
			return;
		}
	}
}

// Function that processes a file lock
void MessageManager::processLockMessage(const FileMessage& m){
	if(!exists(m.targetFile)) {
		std::cout << "No Path\n";
		return;
	}

	constexpr auto writePerms = std::filesystem::perms::owner_write | std::filesystem::perms::group_write | std::filesystem::perms::others_write;
	constexpr auto readPerms = std::filesystem::perms::owner_read | std::filesystem::perms::group_read | std::filesystem::perms::others_read;

	// Determine where the lock file is located
	auto lockPath = lockFilePath(m.targetFile);
	std::cout << "File status." << m.targetFile << std::endl;

	// Path exists so check to see if lock exists.
	std::filesystem::perms check = std::filesystem::status(m.targetFile).permissions();
	std::cout << "Owner Read: " << ((check & std::filesystem::perms::owner_read) != std::filesystem::perms::none ? "r" : "-") << std::endl;
	std::cout << "Group Read: " << ((check & std::filesystem::perms::group_read) != std::filesystem::perms::none ? "r" : "-") << std::endl;
	std::cout << "Others Read: " << ((check & std::filesystem::perms::others_read) != std::filesystem::perms::none ? "r" : "-") << std::endl;
	std::cout << "Owner Write: " << ((check & std::filesystem::perms::owner_write) != std::filesystem::perms::none ? "w" : "-") << std::endl;
	std::cout << "Group Write: " << ((check & std::filesystem::perms::group_write) != std::filesystem::perms::none ? "w" : "-") << std::endl;
	std::cout << "Others Write: " << ((check & std::filesystem::perms::others_write) != std::filesystem::perms::none ? "w" : "-") << std::endl;
	std::cout << "Owner Exec: " << ((check & std::filesystem::perms::owner_exec) != std::filesystem::perms::none ? "x" : "-") << std::endl;
	std::cout << "Group Exec: " << ((check & std::filesystem::perms::group_exec) != std::filesystem::perms::none ? "x" : "-") << std::endl;
	std::cout << "Others Exec: " << ((check & std::filesystem::perms::others_exec) != std::filesystem::perms::none ? "x" : "-") << std::endl;

	// Check for read permissions.
	if((check & readPerms) != std::filesystem::perms::none)
	{
		std::cout << "Owner, Others, or Group may have read permissions.\n";

		// Check to see if there are write permissions which means not locked.
		if((check & writePerms) != std::filesystem::perms::none)

		{
			std::cout << "Owner, Others, or Group may have have write permissions, file not locked.\n";

			// Lock file by writting to file and change permissions.
			std::filesystem::permissions(m.targetFile, writePerms, std::filesystem::perm_options::remove);

			// Open lock file
			std::fstream fout(lockPath, std::ios::out | std::ios::binary);
			boost::archive::binary_oarchive ar(fout, archiveFlags);
			// Save message in file
			ar << m;
			// Save old permissions
			ar << (check & writePerms);

			fout.close();
			std::cout << "File has been written to, file is now locked!!!\n";
		}

		// File has read only permissions so it is locked.
		else
		{
			std::cout << "File is locked.\n";

			FileMessage oldLock;

			{
				std::fstream fin(lockPath, std::ios::in | std::ios::binary);
				boost::archive::binary_iarchive ar(fin, archiveFlags);
				ar >> oldLock;
				ar >> check;
				fin.close();
			}

			if(m.timestamp < oldLock.timestamp) {
				// Open lock file
				std::fstream fout(lockPath, std::ios::out | std::ios::binary);
				boost::archive::binary_oarchive ar(fout, archiveFlags);
				// Save message in file
				ar << m;
				// Save old permissions
				ar << (check & writePerms);

				fout.close();
			}
		}
	}
}

// Function that processes a file unlock
void MessageManager::processUnlockMessage(const FileMessage& m){
	std::cout << "In unlock.\n";

	std::cout << "File status." << m.targetFile << std::endl;

	constexpr auto writePerms = std::filesystem::perms::owner_write | std::filesystem::perms::group_write | std::filesystem::perms::others_write;
	constexpr auto readPerms = std::filesystem::perms::owner_read | std::filesystem::perms::group_read | std::filesystem::perms::others_read;

	auto lockPath = lockFilePath(m.targetFile);
	// If the lock file exists the file is locked
	if(exists(lockPath)){
		FileMessage oldLock;
		std::filesystem::perms permsToAdd;

		std::fstream fin(lockPath, std::ios::in | std::ios::binary);
		boost::archive::binary_iarchive ar(fin, archiveFlags);
		ar >> oldLock;
		ar >> permsToAdd;
		fin.close();

		if(m.originatorNode == oldLock.originatorNode)
		{
			//if path exists check to see if lock exists.
			std::filesystem::perms check = std::filesystem::status(m.targetFile).permissions();
			std::cout << "Owner Read: " << ((check & std::filesystem::perms::owner_read) != std::filesystem::perms::none ? "r" : "-") << std::endl;
			std::cout << "Group Read: " << ((check & std::filesystem::perms::group_read) != std::filesystem::perms::none ? "r" : "-") << std::endl;
			std::cout << "Others Read: " << ((check & std::filesystem::perms::others_read) != std::filesystem::perms::none ? "r" : "-") << std::endl;
			std::cout << "Owner Write: " << ((check & std::filesystem::perms::owner_write) != std::filesystem::perms::none ? "w" : "-") << std::endl;
			std::cout << "Group Write: " << ((check & std::filesystem::perms::group_write) != std::filesystem::perms::none ? "w" : "-") << std::endl;
			std::cout << "Others Write: " << ((check & std::filesystem::perms::others_write) != std::filesystem::perms::none ? "w" : "-") << std::endl;
			std::cout << "Owner Exec: " << ((check & std::filesystem::perms::owner_exec) != std::filesystem::perms::none ? "x" : "-") << std::endl;
			std::cout << "Group Exec: " << ((check & std::filesystem::perms::group_exec) != std::filesystem::perms::none ? "x" : "-") << std::endl;
			std::cout << "Others Exec: " << ((check & std::filesystem::perms::others_exec) != std::filesystem::perms::none ? "x" : "-") << std::endl;

			// Check to see if there are write permissions, if there are then file is unlocked.
			if((check & writePerms) != std::filesystem::perms::none)
			{
				// File is unlocked
				std::cout << "File is unlocked" << std::endl;

			}

			else
			{
				// File is locked add permissions to unlock
				std::filesystem::permissions(m.targetFile, permsToAdd, std::filesystem::perm_options::add);
				std::filesystem::perms check = std::filesystem::status(m.targetFile).permissions();
				std::cout << "File is now unlocked.\n";
				std::cout << "Owner Read: " << ((check & std::filesystem::perms::owner_read) != std::filesystem::perms::none ? "r" : "-") << std::endl;
				std::cout << "Group Read: " << ((check & std::filesystem::perms::group_read) != std::filesystem::perms::none ? "r" : "-") << std::endl;
				std::cout << "Others Read: " << ((check & std::filesystem::perms::others_read) != std::filesystem::perms::none ? "r" : "-") << std::endl;
				std::cout << "Owner Write: " << ((check & std::filesystem::perms::owner_write) != std::filesystem::perms::none ? "w" : "-") << std::endl;
				std::cout << "Group Write: " << ((check & std::filesystem::perms::group_write) != std::filesystem::perms::none ? "w" : "-") << std::endl;
				std::cout << "Others Write: " << ((check & std::filesystem::perms::others_write) != std::filesystem::perms::none ? "w" : "-") << std::endl;
				std::cout << "Owner Exec: " << ((check & std::filesystem::perms::owner_exec) != std::filesystem::perms::none ? "x" : "-") << std::endl;
				std::cout << "Group Exec: " << ((check & std::filesystem::perms::group_exec) != std::filesystem::perms::none ? "x" : "-") << std::endl;
				std::cout << "Others Exec: " << ((check & std::filesystem::perms::others_exec) != std::filesystem::perms::none ? "x" : "-") << std::endl;

				// Erase previous lock file.
				remove(lockPath);

			}
		}
	}
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

	// Update metrics regarding the number of files we have recieved
	totalInitialFiles = m.total;
	recievedInitialFiles++;

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
		sync.total = size;

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

	// Reset file counts (marking that we are not finished connecting to the network)
	recievedInitialFiles = 0;
	totalInitialFiles = 1;
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
