//Annette 
#include <filesystem>

#include "include_everywhere.hpp"



// Parent structure for disconnect and connect.
struct Message
{
    enum Type{lock, unlock, deleteFile, create, change, connect, disconnect} type;  // Action flag must be enumerator.
    uint64_t originatorNode;    // To hold data for originator node. Must be 64 bit int.
};

// Parent structure for Filecreate and FileChange.
struct FileMessage
{
    // To target specific file in path.
    std::filesystem::path targetFile;
    // To obtain timestamp for sweeping.
    std::filesystem::file_time_type timestamp;
};

// Inherited from message and track creation of file.
struct FileCreate: FileMessage
{
    std::string fCreate;   //File content created.
};

//Inherited from message and track all possible file changes.
struct FileChange: FileMessage
{
    std::string fChange;  //File content changed.
};

// Struct for disconnecting nodes.
struct Disconnect: Message
{
    std::vector<uint64_t> disConList;             // Container if needed to keep track of nodes.
};

// Struct for connecting and keeping track of nodes.
struct Connect: Message
{
    std::vector<uint64_t> connectList;           // Container to store all nodes connectee is aware of.
};






