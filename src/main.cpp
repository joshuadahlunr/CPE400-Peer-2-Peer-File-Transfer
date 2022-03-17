#include "ztnode.hpp"
#include "peer.hpp"
#include "monitor.hpp"
#include <csignal>
#include <Argos/Argos.hpp>

// Variable storing our connection to ZeroTier
ZeroTierNode node;
std::jthread listeningThread;
monitor<std::vector<Peer>> peers;

// Callback that shuts down the program when interrupted (ctrl + c in terminal)
void signalCallbackHandler(int signum) {
	// Stop the listening thread
	listeningThread.request_stop();
	listeningThread.join();

	// Terminate program
	std::exit(signum);
}

template<typename Duration = std::chrono::milliseconds>
static zt::Socket connect(const zt::IpAddress& ip, uint16_t port, size_t retryAttemps = 3, Duration timeBetweenAttempts = 100ms) {
	// We change 0 to the maximum number stored in a size_t (heat death of the unversise timeframe attempts)
	if(retryAttemps == 0) retryAttemps--;

	std::cout << retryAttemps << std::endl;

	zt::Socket connectionSocket;
	// Attemp to open a connection <retryAttempts> times
	for(size_t i = 0; i < retryAttemps && !connectionSocket.isOpen(); i++) {
		// Pause between each attempt
		if(i) std::this_thread::sleep_for(timeBetweenAttempts);

		try {
			ZTCPP_THROW_ON_ERROR(connectionSocket.init(zt::SocketDomain::InternetProtocol_IPv6, zt::SocketType::Stream), ZTError);
			ZTCPP_THROW_ON_ERROR(connectionSocket.connect(ip, port), ZTError);
		} catch(ZTError) { /* Do nothing*/ }

		std::cout << i << std::endl;
	}

	// Throw an exception if we failed to connect
	if(!connectionSocket.isOpen())
		throw std::runtime_error("Failed to connect");

	return connectionSocket;
}

int main(int argc, char** argv) {
	// Gracefully terminate when interrupted
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
	node.setup();

	std::cout << "\nConnection IP: >> " << node.getIP() << " <<\n" << std::endl;


	
	// Create a socket that accepts incoming connections
	zt::Socket connectionSocket;
	ZTCPP_THROW_ON_ERROR(connectionSocket.init(zt::SocketDomain::InternetProtocol_IPv6, zt::SocketType::Stream), ZTError);
	ZTCPP_THROW_ON_ERROR(connectionSocket.bind(node.getIP(), port), ZTError);
	ZTCPP_THROW_ON_ERROR(connectionSocket.listen(5), ZTError);
	
	// Start a thread that waits for a connection and then waits for data from that connection
	listeningThread = std::jthread([&](std::stop_token stop){
		std::cout << "Waiting for connection..." << std::endl;
		// Look for a connection until the thread is requested to stop or we open a connection a different way
		while(!stop.stop_requested()){
			// Wait upto 100ms for a connection
			auto pollres = connectionSocket.pollEvents(zt::PollEventBitmask::ReadyToReceiveAny, 100ms);
			ZTCPP_THROW_ON_ERROR(pollres, ZTError);
	
			// If there is a connection, accept it
			if((*pollres & zt::PollEventBitmask::ReadyToReceiveAny) != 0) {
				auto sock = connectionSocket.accept();
				ZTCPP_THROW_ON_ERROR(sock, ZTError);
				peers->emplace_back(std::move(*sock));

				std::cout << "Accepted Connection" << std::endl;
				auto ip = peers.unsafe().back().getSocket().getRemoteIpAddress();
				ZTCPP_THROW_ON_ERROR(ip, ZTError);
				std::cout << "from: " << *ip << std::endl;
			}
		}

		// Close the socket when we stop the thread
		connectionSocket.close();
	});

	// If we have a peer to connect to, add them to our list of peers
	if(remoteIP.isValid())
		peers->emplace_back(std::move(connect(remoteIP, port)));

	std::cout << "got to loop" << std::endl;

	// Keep the program running indefinately, while sending messages to all of the peers
	while(true) {
		std::time_t now = std::time(nullptr);
		std::string message = std::asctime(std::localtime(&now));

		{
			auto peersLock = peers.read_lock();
			for(auto& peer: *peersLock)
				peer.send(message.c_str(), message.size());

			if(!peersLock->empty()) std::cout << "Sent " << message << " - Peers: " << peersLock->size() << std::flush;
		}
		std::this_thread::sleep_for(100ms);
	}

	return 0;
}