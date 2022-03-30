#include "serialize_message.hpp"
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <iostream>
#include <fstream>
#include <sstream>

#include <boost/serialization/vector.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
#include <boost/date_time/posix_time/conversion.hpp>
#include <boost/date_time/posix_time/time_serialize.hpp>
#include <filesystem>



int main (void)
{
	//create stringstream
	std::stringstream ss;

	/*
#pragma region FileMessage Struct Test
	//initialize struct and get file and timestamp
	FileMessage myMessage;
	myMessage.targetFile = "mytest.txt";
	myMessage.timestamp = boost::posix_time::from_time_t(boost::filesystem::last_write_time("mytest.txt"));


	//serialize and stream in
	boost::archive::text_oarchive ao{ ss };
	ao << myMessage;
	//deserialize and stream out
	boost::archive::text_iarchive ai(ss);
	FileMessage newMessage;
	ai >> newMessage;
	//print message on other end
	std::cout << newMessage.targetFile << " @ time " << newMessage.timestamp << std::endl;

	//clear stream for next test
	ss.str(std::string());
#pragma endregion

*/

	/*
#pragma region Message Struct Test

	//Initialize struct and set flag and Uint_64
	Message testMessage;
	testMessage.flag = 0;
	testMessage.originatorNode = 255255255255;

	boost::archive::text_oarchive ao{ ss };
	ao << testMessage;

	boost::archive::text_iarchive ai{ ss };
	Message newTestMessage;
	ai >> newTestMessage;

	std::cout << "flag: " << newTestMessage.flag << " & source: " << newTestMessage.originatorNode << std::endl;

#pragma endregion
	*/

	/*
#pragma region FileCreate struct test
	FileCreate myFile;
	myFile.targetFile = "mytest.txt";
	myFile.timestamp = boost::posix_time::from_time_t(boost::filesystem::last_write_time(myFile.targetFile));
	myFile.fCreate = "Create me";

	boost::archive::binary_oarchive ao{ ss };
	ao << myFile;

	FileCreate myNewFile;
	boost::archive::binary_iarchive ai{ ss };
	ai >> myNewFile;

	std::cout << myNewFile.targetFile << " " << myNewFile.timestamp << " " << myNewFile.fCreate << std::endl;
#pragma endregion
	*/

	/*
#pragma region FileChange struct test
	FileChange myFile;
	myFile.targetFile = "mytest.txt";
	myFile.timestamp = boost::posix_time::from_time_t(boost::filesystem::last_write_time(myFile.targetFile));
	myFile.fCreate = "Create me";

	boost::archive::binary_oarchive ao{ ss };
	ao << myFile;

	FileChange myNewFile;
	boost::archive::binary_iarchive ai{ ss };
	ai >> myNewFile;

	std::cout << myNewFile.targetFile << " " << myNewFile.timestamp << " " << myNewFile.fCreate << std::endl;
#pragma endregion
	*/

	/*
#pragma region Connect struct test
	Connect myConnection;
	uint64_t a = 123;
	uint64_t b = 456;
	uint64_t c = 789;

	myConnection.connectList.push_back(a);
	myConnection.connectList.push_back(b);
	myConnection.connectList.push_back(c);
	myConnection.flag = 2;
	myConnection.originatorNode = 123456789;

	boost::archive::binary_oarchive ao{ ss };
	ao << myConnection;

	Connect myNewConnection;
	boost::archive::binary_iarchive ai{ ss };
	ai >> myNewConnection;

	int size = myNewConnection.connectList.size();
	for (int i = 0; i < size; i++)
	{
		std::cout << myNewConnection.connectList[i] << " ";
	}
	std::cout << myNewConnection.flag << " " << myNewConnection.originatorNode << std::endl;

#pragma endregion
	*/

#pragma region Disconnect struct test
	Connect myDisconnection;
	uint64_t a = 123;
	uint64_t b = 456;
	uint64_t c = 789;

	myDisconnection.connectList.push_back(a);
	myDisconnection.connectList.push_back(b);
	myDisconnection.connectList.push_back(c);
	myDisconnection.flag = 2;
	myDisconnection.originatorNode = 123456789;

	boost::archive::binary_oarchive ao{ ss };
	ao << myDisconnection;

	Connect myNewDisconnection;
	boost::archive::binary_iarchive ai{ ss };
	ai >> myNewDisconnection;

	int size = myNewDisconnection.connectList.size();
	for (int i = 0; i < size; i++)
	{
		std::cout << myNewDisconnection.connectList[i] << " ";
	}
	std::cout << myNewDisconnection.flag << " " << myNewDisconnection.originatorNode << std::endl;

#pragma endregion

	
	return 0;
}
