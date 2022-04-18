/*
	Name: Antonio Massa
	Date: 3/27/2022

	Modified from messages.hpp by Annette
*/
#ifndef __MESSAGES_HPP__
#define __MESSAGES_HPP__

#include <cereal/types/unordered_map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/archives/binary.hpp>
#include <filesystem>

#include "networking_include_everywhere.hpp"


namespace cereal {

	// Support de/serialization of IP Addresses
	template<class Archive>
	void save(Archive& ar, const zt::IpAddress& ip) {
		auto family = ip.getAddressFamily();
		ar ((uint8_t) family);

		if(family == zt::AddressFamily::IPv4)
			ar (ip.getIPv4AddressInNetworkOrder());
		else
			ar (ip.getIPv6AddressInNetworkOrder().bytes);
	}

	template<class Archive>
	void load(Archive& ar, zt::IpAddress& ip) {
		uint8_t _family;
		ar (_family);
		zt::AddressFamily family = (zt::AddressFamily) _family;

		if(family == zt::AddressFamily::IPv4) {
			uint32_t data;
			ar (data);
			ip = zt::IpAddress::ipv4FromBinaryRepresentationInNetworkOrder(&data);
		} else {
			zt::RawIPv6Address data;
			ar (data.bytes);
			ip = zt::IpAddress::ipv6FromBinaryRepresentationInNetworkOrder(&data.bytes);
		}
	}

	// Support de/serialization of std::filesystem::paths
	template<class Archive>
	void save(Archive& ar, const std::filesystem::path& p) {
		std::vector<std::string> parts;
		for(auto part: p.lexically_normal())
			parts.emplace_back(part.string());
		ar (parts);
	}

	template<class Archive>
	void load(Archive& ar, std::filesystem::path& p) {
		std::vector<std::string> parts;
		ar (parts);
		p.clear();
		for(auto& part: parts)
			p /= part;
	}

} // namespace cereal

// Base message class; includes type, routing, and error checking information
struct Message {
	// Action flag must be enumerator.
	enum Type : uint8_t {invalid = 0, lock, unlock, deleteFile, contentChange, initialSync, initialSyncRequest, connect, disconnect, payload, resendRequest, linkLost} type;
	// IP of the destination (may be unspecified to broadcast) node
	zt::IpAddress receiverNode;
	// IP of the source of the previous hop.
	zt::IpAddress senderNode = zt::IpAddress::ipv6Unspecified(); // NOTE: Not serialized
	// IP of the originator node (original source of the message)
	zt::IpAddress originatorNode = zt::IpAddress::ipv6Unspecified();

	// Hash used to verify that a message was transmitted successfully
	size_t messageHash;

	template <typename Archive>
	void serialize(Archive& ar) {
		ar (reference_cast<uint8_t>(type), receiverNode, originatorNode, messageHash);
	}

	// Function that converts the message into a size_t for validation
	size_t hash() const {
		std::string str = hashString();
		size_t hash = 0;
		for(char c: str)
			hash += c;

		return hash;
	}
protected:
	// Virtual function that compiles all the information about a message into a single string that can be "hash"ed
	virtual std::string hashString() const {
		return std::to_string((int)type)
			+ receiverNode.toString()
			+ originatorNode.toString();
	}
};
// Some messages without unique subclassess use the originator node to mark the node that the network can no longer see.


// Message carring an arbitrary string message (mostly used for debugging)
struct PayloadMessage : Message {
	// Arbitrary data this message carries as a payload
	std::string payload;

	template <typename Archive>
	void serialize(Archive& ar) {
		ar (reference_cast<Message>(*this), payload);
	}

protected:
	std::string hashString() const override { return Message::hashString() + payload; }
};
CEREAL_SPECIALIZE_FOR_ALL_ARCHIVES( PayloadMessage, cereal::specialization::member_serialize );

// Message carrying a request that another message be resent
struct ResendRequestMessage : Message {
	// Hash of the message that should be resent
	size_t requestedHash;
	// Original destination IP address
	zt::IpAddress originalDestination;

	template <typename Archive>
	void serialize(Archive& ar) {
		ar (reference_cast<Message>(*this), requestedHash, originalDestination);
	}

protected:
	std::string hashString() const override { return Message::hashString() + std::to_string(requestedHash) + originalDestination.toString(); }
};
CEREAL_SPECIALIZE_FOR_ALL_ARCHIVES( ResendRequestMessage, cereal::specialization::member_serialize );

// Base message type for messages involving a file, contains the file in question and the timestamp it was last modified
struct FileMessage : Message {
	// To target specific file in path.
	std::filesystem::path targetFile;
	// To obtain timestamp for sweeping.
	std::chrono::system_clock::time_point timestamp;

	template<class Archive>
    void save(Archive & ar) const {
		ar (reference_cast<Message>(*this),
		// Save target file as a string
        	targetFile,
		// Save the timestamp as a time_t (long int)
			std::chrono::system_clock::to_time_t(timestamp));
    }
    template<class Archive>
    void load(Archive & ar) {
		ar (reference_cast<Message>(*this));

		// Load target file as a string and then convert it
        ar (targetFile);

		// Load the timestamp as a time_t and then convert it
		time_t tm;
		ar (tm);
		timestamp = std::chrono::system_clock::from_time_t(tm);
    }

protected:
	std::string hashString() const override {
		auto time_t = to_time_t(timestamp);
		return Message::hashString()
			+ targetFile.string()
			+ std::ctime(&time_t);
	}
};
CEREAL_SPECIALIZE_FOR_ALL_ARCHIVES( FileMessage, cereal::specialization::member_load_save );

// File content message containing the contents of the file as a payload
struct FileContentMessage : FileMessage {
	//File content created.
	std::string fileContent;

	template <typename Archive>
	void serialize(Archive& ar) {
		ar (reference_cast<FileMessage>(*this), fileContent);
	}

protected:
	std::string hashString() const override { return FileMessage::hashString() + fileContent; }
};
CEREAL_SPECIALIZE_FOR_ALL_ARCHIVES( FileContentMessage, cereal::specialization::member_serialize );


// File initial sync message, a content message with additional information indicating how many files need to be received before our state is synced with the network
struct FileInitialSyncMessage: FileContentMessage {
	// Variable tracking the total number of files to be synced
	size_t total,
	// Variable tracking which index we are
		index;

	template <typename Archive>
	void serialize(Archive& ar) {
		ar (reference_cast<FileContentMessage>(*this), total, index);
	}

protected:
	std::string hashString() const override { return FileContentMessage::hashString() + std::to_string(total) + std::to_string(index); }
};
CEREAL_SPECIALIZE_FOR_ALL_ARCHIVES( FileInitialSyncMessage, cereal::specialization::member_serialize );

// Message providing extra information needed when we connect: backup gateway ips and the paths we should be sweeping
struct ConnectMessage : Message {
	// List containing backup IPs
	std::vector<std::pair<zt::IpAddress, uint16_t>> backupPeers;

	// List of managed paths
	std::vector<std::filesystem::path> managedPaths;

	template <typename Archive>
	void serialize(Archive& ar) {
		ar (reference_cast<Message>(*this), backupPeers, managedPaths);
	}

	std::string hashString() const override {
		std::string hash = Message::hashString();
		for(auto& [ip, port]: backupPeers)
			hash += ip.toString() + std::to_string(port);
		for(auto& path: managedPaths)
			hash += path.string();
		return hash;
	}
};
CEREAL_SPECIALIZE_FOR_ALL_ARCHIVES( ConnectMessage, cereal::specialization::member_serialize );

#endif // __MESSAGES_HPP__