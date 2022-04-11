/*
	Name: Antonio Massa
	Date: 3/27/2022

	Modified from messages.hpp by Annette
*/
#ifndef __MESSAGES_HPP__
#define __MESSAGES_HPP__

#include <boost/serialization/vector.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/utility.hpp>
#include <filesystem>


#include "networking_include_everywhere.hpp"


// Support de/serialization of IP Addresses
namespace boost::serialization {

	template<class Archive>
	void save(Archive& ar, const zt::IpAddress& ip, const unsigned int version)
	{
		auto family = ip.getAddressFamily();
		ar& (uint8_t) family;

		if(family == zt::AddressFamily::IPv4)
			ar & ip.getIPv4AddressInNetworkOrder();
		else
			ar & ip.getIPv6AddressInNetworkOrder().bytes;
	}

	template<class Archive>
	void load(Archive& ar, zt::IpAddress& ip, const unsigned int version)
	{
		uint8_t _family;
		ar & _family;
		zt::AddressFamily family = (zt::AddressFamily) _family;

		if(family == zt::AddressFamily::IPv4) {
			uint32_t data;
			ar & data;
			ip = zt::IpAddress::ipv4FromBinaryRepresentationInNetworkOrder(&data);
		} else {
			zt::RawIPv6Address data;
			ar & data.bytes;
			ip = zt::IpAddress::ipv6FromBinaryRepresentationInNetworkOrder(&data.bytes);
		}
	}

} // namespace boost::serialization
BOOST_SERIALIZATION_SPLIT_FREE(zt::IpAddress)


struct Message
{
	friend class boost::serialization::access;
	// Action flag must be enumerator.
	enum Type : uint8_t {lock, unlock, deleteFile, create, change, connect, disconnect, payload, linkLost} type;
	// IP of the destination (may be unspecified to broadcast) node
	zt::IpAddress receiverNode;
	// IP of the source of the previous hop.
	zt::IpAddress senderNode = zt::IpAddress::ipv6Unspecified(); // NOTE: Not serialized
	// IP of the originator node (original source of the message)
	zt::IpAddress originatorNode;

	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		ar& reference_cast<uint8_t>(type);
		ar& receiverNode;
		ar& originatorNode;
	}
};

struct PayloadMessage : Message {
	friend class boost::serialization::access;
	// Arbitrary data this message carries as a payload
	std::string payload;

	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		ar& boost::serialization::base_object<Message>(*this);
		ar& payload;
	}
};

struct FileMessage : Message
{
   	friend class boost::serialization::access;
	// To target specific file in path.
	std::filesystem::path targetFile;
	// To obtain timestamp for sweeping.
	std::chrono::system_clock::time_point timestamp;

	template<class Archive>
    void save(Archive & ar, const unsigned int version) const
    {
		ar& boost::serialization::base_object<Message>(*this);
		// Save target file as a string
        ar& targetFile.string();
		// Save the timestamp as a time_t (long int)
		ar& std::chrono::system_clock::to_time_t(timestamp);
    }
    template<class Archive>
    void load(Archive & ar, const unsigned int version)
    {
		ar& boost::serialization::base_object<Message>(*this);

		// Load target file as a string and then convert it
		std::string tempFile;
        ar& tempFile;
		targetFile = tempFile;

		// Load the timestamp as a time_t and then convert it
		time_t tm;
		ar& tm;
		timestamp = std::chrono::system_clock::from_time_t(tm);
    }
    BOOST_SERIALIZATION_SPLIT_MEMBER()
};

struct FileCreateMessage : FileMessage
{
	//File content created.
	std::string fCreate;

	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		ar& boost::serialization::base_object<FileMessage>(*this);
		ar& fCreate;
	}
};

struct FileChangeMessage : FileMessage
{
	//File content changed.
	std::string fChange;


	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		ar& boost::serialization::base_object<FileMessage>(*this);
		ar& fChange;
	}
};

struct ConnectMessage : Message {
	// List containing backup IPs 
	std::vector<std::pair<zt::IpAddress, uint16_t>> backupPeers;

	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		ar& boost::serialization::base_object<Message>(*this);
		ar& backupPeers;
	}
};

struct DisconnectConnectMessage : Message
{
	// Container to store all nodes connectee is aware of.
	std::vector<zt::IpAddress> connectList;

	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		ar& boost::serialization::base_object<Message>(*this);
		ar& connectList;
	}
};

#endif // __MESSAGES_HPP__