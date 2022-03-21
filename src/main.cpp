#include "ztnode.hpp"
#include "peer_manager.hpp"
#include <csignal>
#include <Argos/Argos.hpp>


// Callback that shuts down the program when interrupted (ctrl + c in terminal)
void signalCallbackHandler(int signum) {
	// Terminate program (by calling exit, global variables are destroyed)
	std::exit(signum);
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

	// Establish our connection to ZeroTier
	ZeroTierNode::singleton().setup();

	std::cout << "\nConnection IP: >> " << ZeroTierNode::singleton().getIP() << " <<\n" << std::endl;

	// Initialize the PeerManager singleton (starts listening for connections)
	PeerManager::singleton().setup(ZeroTierNode::singleton().getIP(), port);
	// Acquire a reference to the list of peers 
	auto& peers = PeerManager::singleton().getPeers();

	// If we have a peer to connect to from the command line, add them to our list of peers
	if(remoteIP.isValid())
		peers->emplace_back(std::move(Peer::connect(remoteIP, port)));

	// Keep the program running indefinitely, while sending messages to all of the peers
	size_t i = 0;
	while(true) {
		std::time_t now = std::time(nullptr);
		std::string message = std::to_string(i++) + std::asctime(std::localtime(&now));

		{
			auto peersLock = peers.read_lock();
			for(auto& peer: *peersLock)
				peer.send(message.c_str(), message.size());

			if(!peersLock->empty()) std::cout << "Sent " << message << std::flush;
		}
		std::this_thread::sleep_for(100ms);
	}

	return 0;
}