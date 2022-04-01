#include "ztnode.hpp"
#include "peer_manager.hpp"
#include "file_sweep.hpp"
#include "diff.hpp"
#include <csignal>
#include <Argos/Argos.hpp>
#include <boost/algorithm/string.hpp>


// Callback that shuts down the program when interrupted (ctrl + c in terminal)
void signalCallbackHandler(int signum) {
	// Terminate program (by calling exit, global variables are destroyed)
	std::exit(signum);
}


// Callback called whenever a file is created
void onFileCreated(const std::filesystem::path& path) {
	std::cout << path << " created!" << std::endl;
}

// Callback called whenever a file is modified
void onFileModified(const std::filesystem::path& path) {
	std::cout << path << " modified!" << std::endl;
}

// Callback called whenever a file is deleted
void onFileDeleted(const std::filesystem::path& path) {
	std::cout << path << " deleted!" << std::endl;
}

int main(int argc, char** argv) {
	// Gracefully terminate when interrupted (ctrl + c in terminal)
	signal(SIGINT, signalCallbackHandler);

	// Parse the command line
	const argos::ParsedArguments args = argos::ArgumentParser(argv[0])
        .about("Command line utility that syncsronizes a filesystem across a peer-2-peer network.")
		.add(argos::Argument("FOLDER").help("The folder/directory to be synchronized across the network."))
        .add(argos::Argument("IP").optional(true)
            .help("IP address of a peer on the network we wish to join. (If not set, a new network is established)"))
        .add(argos::Option{"-p", "--port"}.argument("PORT")
            .help("Optional port number to connect to (default=" + std::to_string(defaultPort) + ")"))
        .parse(argc, argv);
	uint16_t port = args.value("-p").as_uint(defaultPort);
	auto remoteIP = zt::IpAddress::ipv6FromString(args.value("IP").as_string());
	std::vector<std::filesystem::path> folders; boost::split(folders, args.value("FOLDER").as_string(), boost::is_any_of(","));


	{
		std::string a = "Hello Bob!", b = "Hello Barb!";
		auto diff = extractDiff(a, b);

		std::cout << applyDiff(a, diff) << std::endl; // Converts a to "Hello Barb!"
		std::cout << undoDiff(b, diff) << std::endl; // Converts b to "Hello Bob!"
	}


	// Setup the networking components in a thread (it takes a while so we also tidy up the filesystem at the same time)
	std::thread networkSetupThread([port]{
		// Establish our connection to ZeroTier
		ZeroTierNode::singleton().setup();

		// Initialize the PeerManager singleton (starts listening for connections)
		PeerManager::singleton().setup(ZeroTierNode::singleton().getIP(), port);
	});

	// Create a filesystem sweeper that scan the folders from command line, and repoerts its results to the onFile* functions in this file
	FilesystemSweeper sweeper{folders, onFileCreated, onFileModified, onFileDeleted};
	sweeper.setup();

	// Wait for the node setup to finish
	networkSetupThread.join();
	std::cout << "\nConnection IP: >> " << ZeroTierNode::singleton().getIP() << " <<\n" << std::endl;


	// Acquire a reference to the list of peers
	auto& peers = PeerManager::singleton().getPeers();

	// If we have a peer to connect to from the command line, add them to our list of peers
	if(remoteIP.isValid())
		peers->emplace_back(std::move(Peer::connect(remoteIP, port)));

	// Sweep for the first time then start the main loop
	sweeper.sweep(/*total*/ true);


	while(true) {
		// Generate a message with an arbitrary payload
		PayloadMessage m;
		m.type = Message::Type::payload;
		std::time_t now = std::time(nullptr);
		m.payload = std::to_string(sweeper.iteration) + std::asctime(std::localtime(&now));
		// Send the payload message
		PeerManager::singleton().send(m);

		// Sweep the file system, with a total sweep every 10 iterations (10 seconds)
		sweeper.totalSweepEveryN(10);

		std::this_thread::sleep_for(1s);
	}

	return 0;
}