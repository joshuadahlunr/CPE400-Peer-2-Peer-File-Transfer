/*
	Name: Antonio Massa
	Date: 3/27/2022

	Modified from messages.hpp by Annette
*/
#include <boost/serialization/vector.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <filesystem>


#include "include_everywhere.hpp"

struct Message
{
	friend class boost::serialization::access;
	// Action flag must be enumerator.
	enum Type : uint8_t {lock, unlock, deleteFile, create, change, connect, disconnect} type;
	// To hold data for originator node. Must be 64 bit int.
	uint64_t originatorNode;

	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		ar& reference_cast<uint8_t>(type);
		ar& originatorNode;
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

struct ConnectMessage : Message
{
	// Container to store all nodes connectee is aware of.
	std::vector<uint64_t> connectList;
	int size;

	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		size = connectList.size();
		ar& boost::serialization::base_object<Message>(*this);
		ar& connectList;
	}
};

struct DisconnectMessage : Message
{
	// Container if needed to keep track of nodes.
	std::vector<uint64_t> disConList;
	int size;

	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		size = disConList.size();
		ar& boost::serialization::base_object<Message>(*this);
		ar& disConList;
	}
};
