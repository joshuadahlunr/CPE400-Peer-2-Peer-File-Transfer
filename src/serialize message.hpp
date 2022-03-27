/*
	Name: Antonio Massa
	Date: 3/27/2022

	Modified from messages.hpp by Annette
*/
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/vector.hpp >
#include <boost/serialization/base_object.hpp>

#include <filesystem>
#include "include_everywhere.hpp"

class Message
{
	friend class boost::serialization::access;
};
class serialized_message : public Message public FileMessage
{
	friend class boost::serialization::access;

	// Action flag must be enumerator.
	enum Flag { lock, unlock, deleteFile, create, change, connect, disconnect } flag;
	// To hold data for originator node. Must be 64 bit int.
	uint64_t originatorNode;    

	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		int a = (int)flag;
		ar& a;
		ar& originatorNode;
	}
public:
	Message() {};
	Message(Flag flag, uint64_t originatorNode) {};
	~Message() {};
};

class FileMessage
{
	friend class boost::serialization::access;

	// To target specific file in path.
	std::filesystem::path targetFile;
	// To obtain timestamp for sweeping.
	std::filesystem::file_time_type timestamp;

	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		ar& targeFile;
		ar& timestamp;
	}
public:
	FileMessage() {};
	FileMessage(std::filesystem::path TargetFile, std::filesystem::file_time_type Timestamp) 
		: targetfile(TargetFile), timestamp(Timestamp) {};
	~FileMessage() {};
};

class FileCreate : FileMessage
{
	friend class boost::serialization::access;

	//File content created.
	std::string fCreate;

	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		ar& boost::serialization::base_object<FileMessage>(*this);
		ar& fCreate;
	}
public:
	FileCreate() {};
	FileCreate(std::filesystem::path TargetFile, std::filesystem::file_time_type Timestamp, std::string FCreate) 
		: targetfile(TargetFile), timestamp(Timestamp), fCreate(FCreate) {};
	~FileCreate() {};
};

class FileChange : FileMessage
{
	friend class boost::serialization::access;

	//File content changed.
	std::string fChange;


	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		ar& boost::serialization::base_object<FileMessage>(*this);
		ar& fChange;
	}
public:
	FileChange() {};
	FileChange(std::filesystem::path TargetFile, std::filesystem::file_time_type Timestamp, std::string FChange) 
		: targetfile(TargetFile), timestamp(Timestamp), fChange(FChange) {};
	~FileChange() {};
};

class Disconnect : Message
{
	friend class boost::serialization::access;

	// Container if needed to keep track of nodes.
	std::vector<uint_t> disConList;
	int size;

	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		size = disConList.size();
		ar& boost::serialization::base_object<Message>(*this);
		ar& size;
		for (int i = 0; i < size; i++)
		{
			ar& disConList[i];
		}
	}
public:
	Disconnect() {};
	Disconnect(Flag flagType, uint64_t OriginatorNode, std::vector<uint_t> DisConList)
		: flag(flagType), originatorNode(OriginatorNode), disConList(DisConList) {};
	~Disconnect() {};
};

class Connect : Message
{
	friend class boost::serialization::access;

	// Container to store all nodes connectee is aware of.
	std::vector<uint64_t> connectList;
	int size;

	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		size = connectList.size();
		ar& boost::serialization::base_object<Message>(*this);
		ar& size;
		for (int i = 0; i < size; i++)
		{
			ar& connectList[i];
		}
	}
public:
	Connect() {};
	Connect(Flag flagType, uint64_t OriginatorNode, std::vector<uint_t> ConnectList)
		: flag(flagType), originatorNode(OriginatorNode), connectList(ConnectList) {};
	~Connect() {};
};