/*
	Name: Joshua Dahl, Antonio Massa, and Annette McDonough
	Date: 4/19/22

	Implementation of the message manager, provides processing for the different remote messages
*/

#include "peer_manager.hpp"
#include "message_manager.hpp"

#include <fstream>

// Function that calculates the path to a file's lock file
auto lockFilePath(const std::filesystem::path& p) {
	auto lockPath = wntsPath(p);
	return lockPath.remove_filename() / (".lock." + p.filename().string());
}

// Function that loads the data from a lock file
auto loadLockFile(const std::filesystem::path& p) {
	std::pair<FileMessage, std::filesystem::perms> out;

	std::ifstream fin(lockFilePath(p), std::ios::binary);
	cereal::BinaryInputArchive ar(fin);
	ar (out.first, out.second);
	fin.close();

	return out;
}

// Validate the provided message, returns true if the hashes match, requests a resend and returns false otherwise
bool MessageManager::validateMessageHash(const Message& m, uint8_t offset /*= 0*/) const {
	if(useVerboseOutput) std::cout << m.messageHash << " - " << (m.hash() + offset) << std::endl;
	if(m.messageHash != m.hash() + offset) {
		if(useVerboseOutput) std::cerr << "INVALID MESSAGE" << std::endl << std::endl;
		ResendRequestMessage resend;
		resend.type = Message::Type::resendRequest;
		resend.requestedHash = m.messageHash;
		resend.originalDestination = m.receiverNode;
		// Request that the message be resent by its sender
		PeerManager::singleton().send(resend, m.senderNode);
		return false;
	}
	return true;
}

// Destructor is responsible for cleaning up
MessageManager::~MessageManager (){
	// Process all of the messages currently waiting in the queue
	// NOTE: The peer manager be shutdown first, so we don't need to worry about additional messages while we are trying to shutdown
	while(!messageQueue->empty())
		processNextMessage();

	// Make sure that none of the folders are considered locked (prevents weird permission errors on the next run of the program)
	for(auto path: enumerateAllFiles(*folders))
		if(exists(lockFilePath(path))) {
			auto [_, permsToAdd] = loadLockFile(path);
			std::filesystem::permissions(path, permsToAdd, std::filesystem::perm_options::add);
		}
}


// -- Message Processing Functions --


// Function that processes a resend request
bool MessageManager::processResendRequestMessage(const ResendRequestMessage& request) {
	// If we are the source of the resend abort
	if(request.originatorNode == ZeroTierNode::singleton().getIP())
		return true;

	// Find the message that needs to be resent in the old message cache, then resend it
	for(auto& m: oldMessages) {
		if(m->messageHash == request.requestedHash) {
			switch(m->type) {
			break; case Message::Type::payload:				PeerManager::singleton().send(reference_cast<PayloadMessage>(*m), request.originalDestination);
			// break; case Message::Type::resendRequest:		PeerManager::singleton().send(reference_cast<ResendRequestMessage>(*m), request.originalDestination);
			break; case Message::Type::lock:				PeerManager::singleton().send(reference_cast<FileMessage>(*m), request.originalDestination);
			break; case Message::Type::unlock:				PeerManager::singleton().send(reference_cast<FileMessage>(*m), request.originalDestination);
			break; case Message::Type::deleteFile:			PeerManager::singleton().send(reference_cast<FileMessage>(*m), request.originalDestination);
			break; case Message::Type::contentChange:		PeerManager::singleton().send(reference_cast<FileContentMessage>(*m), request.originalDestination);
			break; case Message::Type::initialSync:			PeerManager::singleton().send(reference_cast<FileInitialSyncMessage>(*m), request.originalDestination);
			break; case Message::Type::initialSyncRequest:	PeerManager::singleton().send(reference_cast<Message>(*m), request.originalDestination);
			break; case Message::Type::connect:				PeerManager::singleton().send(reference_cast<ConnectMessage>(*m), request.originalDestination);
			break; case Message::Type::disconnect:			PeerManager::singleton().send(reference_cast<Message>(*m), request.originalDestination);
			break; case Message::Type::linkLost:			PeerManager::singleton().send(reference_cast<Message>(*m), request.originalDestination);
			break; default:
				throw std::runtime_error("Unrecognized message type");
			}

			// Message was successfully processed, no need to add back to queue
			return true;
		}
	}

	// Message was successfully processed, no need to add back to queue
	return true;
}

// Constants for masking relevant permissions
constexpr auto writePerms = std::filesystem::perms::owner_write | std::filesystem::perms::group_write | std::filesystem::perms::others_write;
constexpr auto readPerms = std::filesystem::perms::owner_read | std::filesystem::perms::group_read | std::filesystem::perms::others_read;

// Function that processes a file lock
bool MessageManager::processLockMessage(const FileMessage& m) {
	// If we are still connecting to the network, process this message later
	if(!isFinishedConnecting())
		return false;

	// If the target file doesn't exist we can't lock it
	if(!exists(m.targetFile))
		return true;


	// Determine where the lock file is located
	auto lockPath = lockFilePath(m.targetFile);

	// Check for read permissions.
	std::filesystem::perms check = std::filesystem::status(m.targetFile).permissions();
	if((check & readPerms) != std::filesystem::perms::none) {
		// Check to see if there are write permissions or the lock file doesn't exist which means not locked.
		if((check & writePerms) != std::filesystem::perms::none || !exists(lockPath)) {
			// Prevent us from writing to the file (unless we took the lock)
			if(m.originatorNode != ZeroTierNode::singleton().getIP())
				std::filesystem::permissions(m.targetFile, writePerms, std::filesystem::perm_options::remove);

			// Open lock file
			auto folder = lockPath;
			create_directories(folder.remove_filename());
			std::ofstream fout(lockPath, std::ios::binary);
			cereal::BinaryOutputArchive ar(fout);
			// Save message in file and save old permissions
			ar(m, (check & writePerms));

			fout.close();
		}

		// File has read only permissions so it is locked.
		else {
			FileMessage oldLock;
			std::tie(oldLock, check) = loadLockFile(m.targetFile);

			if(m.timestamp < oldLock.timestamp) {
				// Open lock file
				std::ofstream fout(lockPath, std::ios::binary);
				auto folder = lockPath;
				create_directories(folder.remove_filename());
				cereal::BinaryOutputArchive ar(fout);
				// Save message in file and save old permissions
				ar (m, (check & writePerms));

				fout.close();
			}
		}
	}

	// Message was successfully processed, no need to add back to queue
	return true;
}

// Function that processes a file unlock
bool MessageManager::processUnlockMessage(const FileMessage& m) {
	// If we are still connecting to the network, process this message later
	if(!isFinishedConnecting())
		return false;

	// If the lock file exists the file is locked
	auto lockPath = lockFilePath(m.targetFile);
	if(exists(lockPath)) {
		auto [oldLock, permsToAdd] = loadLockFile(m.targetFile);

		if(m.originatorNode == oldLock.originatorNode) {
			//if path exists check to see if lock exists.
			std::filesystem::perms check = std::filesystem::status(m.targetFile).permissions();

			// Check to see if there are write permissions, if there are then file is unlocked.
			if((check & writePerms) == std::filesystem::perms::none) {
				// File is locked add permissions to unlock
				std::filesystem::permissions(m.targetFile, permsToAdd, std::filesystem::perm_options::add);

				// Erase lock file.
				remove(lockPath);
			}
		}
	}

	// Message was successfully processed, no need to add back to queue
	return true;
}

// Function that processes a file delete
bool MessageManager::processDeleteFileMessage(const FileMessage& m) {
	// If we are still connecting to the network, process this message later
	if(!isFinishedConnecting())
		return false;

	// Make sure the file isn't locked
	if(exists(lockFilePath(m.targetFile))) {
		auto [lock, _] = loadLockFile(m.targetFile);

		// The file can't be deleted because a lock already exists
		if(lock.originatorNode != ZeroTierNode::singleton().getIP())
			return true;
	}

	// Delete the file, its backup, and its lock
	remove(m.targetFile);
	remove(lockFilePath(m.targetFile));
	remove(wntsPath(m.targetFile));

	// Message was successfully processed, no need to add back to queue
	return true;
}

// Function that processes a new file content message
bool MessageManager::processContentFileMessage(const FileContentMessage& m) {
	// If we are still connecting to the network, process this message later
	if(!isFinishedConnecting())
		return false;

	// Make sure the file isn't locked
	if(exists(lockFilePath(m.targetFile))) {
		auto [lock, _] = loadLockFile(m.targetFile);

		// The file can't be deleted because a lock already exists
		if(lock.originatorNode != ZeroTierNode::singleton().getIP())
			return true;
	}

	// Save the file's content (creating any nessicary intermediate directories)
	auto folder = m.targetFile;
	create_directories(folder.remove_filename());
	std::ofstream fout(m.targetFile);
	fout << m.fileContent;
	fout.close();

	// Message was successfully processed, no need to add back to queue
	return true;
}

// Function that processes an initial file sync
bool MessageManager::processInitialFileSyncMessage(const FileInitialSyncMessage& m) {
	// Update metrics regarding the number of files we have received
	totalInitialFiles = m.total;
	receivedInitialFiles++;

	// Create intermediate directories
	auto folder = m.targetFile;
	create_directories(folder.remove_filename());
	// Write the content of the file to disk
	std::ofstream fout(m.targetFile);
	fout << m.fileContent;
	fout.close();

	// Message was successfully processed, no need to add back to queue
	return true;
}

// Function that processes an initial file sync request
bool MessageManager::processInitialFileSyncRequestMessage(const Message& m) {
	// If we are still connecting to the network, process this message later
	if(!isFinishedConnecting())
		return false;

	// Send the content of every managed file to the newly connected node
	auto paths = enumerateAllFiles(*folders);
	for(size_t i = 0, size = paths.size(); i < size; i++) {
		FileInitialSyncMessage sync;
		sync.type = Message::Type::initialSync;
		sync.targetFile = paths[i];
		sync.timestamp = std::chrono::system_clock::now();
		sync.index = i;
		sync.total = size;

		std::ifstream fin(sync.targetFile);
		std::getline(fin, sync.fileContent, '\0');
		fin.close();

		PeerManager::singleton().send(sync, m.originatorNode);

		// If the file is locked also send a lock message
		if(exists(lockFilePath(sync.targetFile))) {
			auto [lock, _] = loadLockFile(sync.targetFile);
			PeerManager::singleton().send(lock, m.originatorNode);
		}
	}

	// Message was successfully processed, no need to add back to queue
	return true;
}

// Function that processes connection info from the gateway peer
bool MessageManager::processConnectMessage(const ConnectMessage& m) {
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
	receivedInitialFiles = 0;
	totalInitialFiles = 1;

	// Message was successfully processed, no need to add back to queue
	return true;
}

// Function that handles losing our link to a peer
bool MessageManager::processLinkLostMessage(const Message& m) {
	zt::IpAddress removedIP; {
		auto peerLock = PeerManager::singleton().getPeers().write_lock();
		// Find the Peer that disconnected
		size_t index = -1;
		for(size_t i = 0; i < peerLock->size(); i++)
			if(peerLock[i].getRemoteIP() == m.originatorNode) {
				index = i;
				break;
			}
		if(index != std::numeric_limits<size_t>::max()) {
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

	// Message was successfully processed, no need to add back to queue
	return true;
}

// Function that handles a peer disconnect
bool MessageManager::processDisconnectMessage(const Message& m) {
	// If we are still connecting to the network, process this message later
	if(!isFinishedConnecting())
		return false;

	// Remove the disconnected Peer as a backup Peer
	auto& backupPeers = PeerManager::singleton().backupPeers;
	for(size_t peer = 0; peer < backupPeers.size(); peer++) {
		auto& [backupIP, _] = backupPeers[peer];
		if(backupIP == m.originatorNode)
			backupPeers.erase(backupPeers.begin() + peer);
	}

	// Send ourselves an unlock message for every file (any files locked by different peers should have the message rejected)
	auto paths = enumerateAllFiles(*folders);
	for(auto& path: paths) {
		FileMessage unlock;
		unlock.type = Message::Type::unlock;
		unlock.targetFile = path;
		unlock.originatorNode = m.originatorNode; // Mark that the disconnecting node is requesting the unlock
		PeerManager::singleton().send(unlock, zt::IpAddress::ipv6Loopback()); // Loopback = only send to self
	}

	// Message was successfully processed, no need to add back to queue
	return true;
}
