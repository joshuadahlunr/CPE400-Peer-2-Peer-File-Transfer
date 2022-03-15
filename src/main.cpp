#include "ztnode.hpp"
#include "jthread.hpp"
#include <csignal>
#include <cstring>
#include <Argos/Argos.hpp>

// Variable storing our connection to ZeroTier
ZeroTierNode node;
std::jthread listeningThread;

// Callback that shuts down the program when interrupted (ctrl + c in terminal)
void signalCallbackHandler(int signum) {
	// Stop the listening thread
	listeningThread.request_stop();
	listeningThread.join();

	// Terminate program
	std::exit(signum);
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
	zt::Socket listeningSocket;
	ZTCPP_THROW_ON_ERROR(listeningSocket.init(zt::SocketDomain::InternetProtocol_IPv6, zt::SocketType::Stream), std::runtime_error);
	ZTCPP_THROW_ON_ERROR(listeningSocket.bind(node.getIP(), port), std::runtime_error);
	ZTCPP_THROW_ON_ERROR(listeningSocket.listen(1), std::runtime_error);

	// Create a socket that sends messages
	zt::Socket sendingSocket;
	if(remoteIP.isValid()){
		ZTCPP_THROW_ON_ERROR(sendingSocket.init(zt::SocketDomain::InternetProtocol_IPv6, zt::SocketType::Stream), std::runtime_error);
		ZTCPP_THROW_ON_ERROR(sendingSocket.connect(remoteIP, port), std::runtime_error);
	}
	
	// Start a thread that waits for a connection and then waits for data from that connection
	listeningThread = std::jthread([&](std::stop_token stop){
		std::byte recvBuf[1024];
		std::memset(recvBuf, 0, sizeof(recvBuf));

		std::cout << "Waiting for connection..." << std::endl;
		auto s = listeningSocket.accept();
		ZTCPP_THROW_ON_ERROR(s, std::runtime_error);
		listeningSocket = std::move(*s);

		std::cout << "Accepted Connection" << std::endl;
		auto ip = listeningSocket.getRemoteIpAddress();
		ZTCPP_THROW_ON_ERROR(ip, std::runtime_error);

		// If we aren't sending data, start sending data to the connecred client
		if(!sendingSocket.isOpen()){
			ZTCPP_THROW_ON_ERROR(sendingSocket.init(zt::SocketDomain::InternetProtocol_IPv6, zt::SocketType::Stream), std::runtime_error);
			ZTCPP_THROW_ON_ERROR(sendingSocket.connect(*ip, port), std::runtime_error);
		}

		// Loop unil the thread is requested to stop
		while(!stop.stop_requested()){
			// Wait 100ms for data
			auto pollres = listeningSocket.pollEvents(zt::PollEventBitmask::ReadyToReceiveAny, 100ms);
			ZTCPP_THROW_ON_ERROR(pollres, std::runtime_error);
	
			// If there is data ready to be recieved...
			if((*pollres & zt::PollEventBitmask::ReadyToReceiveAny) != 0) {
				// Recieve the data
				zt::IpAddress remoteIp;
				std::uint16_t remotePort;
				auto res = listeningSocket.receiveFrom(recvBuf, sizeof(recvBuf), remoteIp, remotePort);
				ZTCPP_THROW_ON_ERROR(res, std::runtime_error);

				// And print it out
				std::cout << "Received " << *res << " bytes from " << remoteIp << ":" << remotePort << ";\n"
							<< "		Message: " << (char*) recvBuf << std::endl;
			}
		}

		// Close the sockets when we stop the thread
		listeningSocket.close();
		sendingSocket.close();
	});

	// Keep the program running indefinately, while sending messages
	while(true) {
		if(sendingSocket.isOpen()) {
			std::time_t now = std::time(nullptr);
			std::string message = std::asctime(std::localtime(&now));
			sendingSocket.send(message.c_str(), 13);
			std::cout << "Sent " << message << "!" << std::endl;

			std::this_thread::sleep_for(100ms);
		}
	}

	return 0;
}