/*
	Name: Antonio Massa
	Date: 3/27/2022

	Modified from messages.hpp by Annette
*/
#include <boost/serialization/vector.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
#include <boost/date_time/posix_time/conversion.hpp>
#include <boost/date_time/posix_time/time_serialize.hpp>
#include <filesystem>


#include <filesystem>
//#include "include_everywhere.hpp"

struct Message
{
	friend class boost::serialization::access;
	// Action flag must be enumerator.
	int flag;
	// To hold data for originator node. Must be 64 bit int.
	uint64_t originatorNode;    

	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		ar& flag;
		ar& originatorNode;
	}
};

struct FileMessage
{
   	 friend class boost::serialization::access;
	// To target specific file in path.
	std::string targetFile;
	// To obtain timestamp for sweeping.
	boost::posix_time::ptime timestamp;

	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		ar& targetFile;
		ar& timestamp;
	}
};

struct FileCreate : FileMessage
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

struct FileChange : FileMessage
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

struct Disconnect : Message
{
	// Container if needed to keep track of nodes.
	std::vector<uint64_t> disConList;
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
};

struct Connect : Message
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
