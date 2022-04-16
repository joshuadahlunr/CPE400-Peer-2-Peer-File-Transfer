#include "ztnode.hpp"
#include "peer_manager.hpp"
#include "message_manager.hpp"
#include "file_sweep.hpp"
#include <csignal>
#include <Argos/Argos.hpp>
#include <boost/algorithm/string.hpp>
#include <fstream>


// Callback that shuts down the program when interrupted (ctrl + c in terminal)
void signalCallbackHandler(int signum) {
	// Terminate program (by calling exit, global variables are destroyed)
	std::exit(signum);
}


// Callback called whenever a file is created or modified
void onFileCreatedOrModified(const std::filesystem::path& path) {
	// Propagate the file's creation
	FileContentMessage m;
	m.type = Message::Type::contentChange;
	m.targetFile = path;
	m.timestamp = convertTimepoint<std::chrono::system_clock::time_point>(last_write_time(path));

	// Read the entire content of the file
	std::ifstream fin(path);
	std::getline(fin, m.fileContent, '\0');
	fin.close();

	PeerManager::singleton().send(m); // Broadcast the message
}

// Callback called whenever a file is deleted
void onFileDeleted(const std::filesystem::path& path) {
	// Propagate the file's deletion
	FileMessage m;
	m.type = Message::Type::deleteFile;
	m.targetFile = path;
	m.timestamp = std::chrono::system_clock::now();
	PeerManager::singleton().send(m); // Broadcast the message
}

// Callback called whenever a file is fast-tracked
void onFileFastTracked(const std::filesystem::path& path) {
	// Propagate a lock through the network
	FileMessage m;
	m.type = Message::Type::lock;
	m.targetFile = path;
	m.timestamp = convertTimepoint<std::chrono::system_clock::time_point>(last_write_time(path));
	PeerManager::singleton().send(m); // Broadcast the message
}

// Callback called whenever a file is unfast-tracked
void onFileUnFastTracked(const std::filesystem::path& path) {
	// Propagate an unlock through the network
	FileMessage m;
	m.type = Message::Type::unlock;
	m.targetFile = path;
	m.timestamp = convertTimepoint<std::chrono::system_clock::time_point>(last_write_time(path));
	PeerManager::singleton().send(m); // Broadcast the message
}

int main(int argc, char** argv) {
	// Gracefully terminate when interrupted (ctrl + c in terminal)
	signal(SIGINT, signalCallbackHandler);

	// Parse the command line
	#define COMMAND_LINE_ARGS argos::ArgumentParser(argv[0])\
        .about("Command line utility that syncsronizes a filesystem across a peer-2-peer network.")\
		.add(argos::Option{"-f", "--folders"}.argument("FOLDER")\
			.help("The folder/directory to be synchronized across the network."))\
        .add(argos::Option{"-c", "--connect", "--remote-address"}.argument("IP")\
            .help("IP address of a peer on the network we wish to join. (If not set, a new network is established)"))\
        .add(argos::Option{"-p", "--port"}.argument("PORT")\
            .help("Optional port number to connect to (default=" + std::to_string(defaultPort) + ")"))
	const argos::ParsedArguments args = COMMAND_LINE_ARGS.parse(argc, argv);
	uint16_t port = args.value("-p").as_uint(defaultPort);
	auto remoteIP = zt::IpAddress::ipv6FromString(args.value("-c").as_string());
	std::vector<std::filesystem::path> folders; boost::split(folders, args.value("-f").as_string(), boost::is_any_of(","));

	// If neither a list of folders nor remote IP are specified, error
	if(folders.size() == 1 && folders[0] == "" && !remoteIP.isValid()) {
		std::cerr << "wnts: Either a list of folders to manage, or the IP of a node on an existing network must be provided" << std::endl;
		// Display the usage
		std::array<const char*, 2> dummy = {argv[0], "fail!"};
		auto _ = COMMAND_LINE_ARGS.parse(2, (char**) dummy.data());
	}
	// If a remote IP is provided, clear the list of folders
	if(remoteIP.isValid()) folders.clear();

	// If any of the specified folders don't exist... error, or make sure all of the paths are relative
	for(auto& path: folders)
		if(!exists(path)) {
			std::cerr << "wnts: Target folder: " << path << " doesn't exist!" << std::endl;
			// Display the usage
			std::array<const char*, 2> dummy = {argv[0], "fail!"};
			auto _ = COMMAND_LINE_ARGS.parse(2, (char**) dummy.data());
		} else
			path = relative(path);



	// Setup the networking components in a thread (it takes a while so we also tidy up the filesystem at the same time)
	std::thread networkSetupThread([&folders, port] {
		// Link the message manager's folders
		MessageManager::singleton().setup(folders);

		// Establish our connection to ZeroTier
		ZeroTierNode::singleton().setup();

		// Initialize the PeerManager singleton (starts listening for connections)
		PeerManager::singleton().setup(ZeroTierNode::singleton().getIP(), port);
	});

	// Create a filesystem sweeper that scan the folders from command line, and repoerts its results to the onFile* functions in this file
	FilesystemSweeper sweeper{folders, onFileCreatedOrModified, onFileCreatedOrModified, onFileDeleted, onFileFastTracked, onFileUnFastTracked};
	sweeper.setup();

	// Wait for the node setup to finish
	networkSetupThread.join();
	std::cout << "\nConnection IP: >> " << ZeroTierNode::singleton().getIP() << " <<\n" << std::endl;


	// Acquire a reference to the list of peers
	auto& peers = PeerManager::singleton().getPeers();

	// If we have a peer to connect to from the command line, add them to our list of peers
	if(remoteIP.isValid()) {
		peers->emplace_back(std::move(Peer::connect(remoteIP, port)));
		PeerManager::singleton().setGatewayIP(remoteIP); // Mark the remote IP as our "gateway" to the rest of the network

	// If we are starting a network, tell the message manager that we are completely connected
	} else
		MessageManager::singleton().totalInitialFiles = 0;


	// Sweep for the first time then start the main loop
	sweeper.sweep(/*total*/ true);
	while(true) {
		// Note the time this loop iteration starts
		auto start = std::chrono::system_clock::now();

		// Sweep the file system, with a total sweep every 10 iterations (10 seconds)
		sweeper.totalSweepEveryN(10);

		// Process messages until a second has elapsed since the start of the loop
		while(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start).count() < 1000)
			MessageManager::singleton().processNextMessage();
	}

	return 0;
}