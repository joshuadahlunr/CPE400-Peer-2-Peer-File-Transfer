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
	zt::Socket connectionSocket;
	ZTCPP_THROW_ON_ERROR(connectionSocket.init(zt::SocketDomain::InternetProtocol_IPv6, zt::SocketType::Stream), std::runtime_error);
	ZTCPP_THROW_ON_ERROR(connectionSocket.bind(node.getIP(), port), std::runtime_error);
	ZTCPP_THROW_ON_ERROR(connectionSocket.listen(1), std::runtime_error);

	// Create a socket that sends messages
	zt::Socket dataSocket;
	if(remoteIP.isValid()){
		ZTCPP_THROW_ON_ERROR(dataSocket.init(zt::SocketDomain::InternetProtocol_IPv6, zt::SocketType::Stream), std::runtime_error);
		ZTCPP_THROW_ON_ERROR(dataSocket.connect(remoteIP, port), std::runtime_error);
	}
	
	// Start a thread that waits for a connection and then waits for data from that connection
	listeningThread = std::jthread([&](std::stop_token stop){
		std::cout << "Waiting for connection..." << std::endl;
		// Look for a connection until the thread is requested to stop or we open a connection a different way
		while(!stop.stop_requested() && !dataSocket.isOpen()){
			// Wait upto 100ms for a connection
			auto pollres = connectionSocket.pollEvents(zt::PollEventBitmask::ReadyToReceiveAny, 100ms);
			ZTCPP_THROW_ON_ERROR(pollres, std::runtime_error);
	
			// If there is a connection, accept it
			if((*pollres & zt::PollEventBitmask::ReadyToReceiveAny) != 0) {
				auto sock = connectionSocket.accept();
				ZTCPP_THROW_ON_ERROR(sock, std::runtime_error);
				dataSocket = std::move(*sock);

				while(!dataSocket.isOpen())
					std::this_thread::sleep_for(100ms);

				std::cout << "Accepted Connection" << std::endl;
				auto ip = dataSocket.getRemoteIpAddress();
				ZTCPP_THROW_ON_ERROR(ip, std::runtime_error);
				std::cout << "from: " << *ip << std::endl;
				break;
			}
		}

		std::byte recvBuf[30];
		std::memset(recvBuf, 0, sizeof(recvBuf));

		// Loop unil the thread is requested to stop
		while(!stop.stop_requested()){
			// Wait upto 100ms for data
			auto pollres = dataSocket.pollEvents(zt::PollEventBitmask::ReadyToReceiveAny, 100ms);
			ZTCPP_THROW_ON_ERROR(pollres, std::runtime_error);
	
			// If there is data ready to be recieved...
			if((*pollres & zt::PollEventBitmask::ReadyToReceiveAny) != 0) {
				// Recieve the data
				zt::IpAddress remoteIP;
				std::uint16_t remotePort;
				auto res = dataSocket.receiveFrom(recvBuf, sizeof(recvBuf), remoteIP, remotePort);
				ZTCPP_THROW_ON_ERROR(res, std::runtime_error);

				// And print it out
				std::cout << "Received " << *res << " bytes from " << remoteIP << ":" << remotePort << ";\n"
							<< "		Message: " << (char*) recvBuf << std::endl;

				// Clear the buffer
				std::memset(recvBuf, 0, sizeof(recvBuf));
			}
		}

		// Close the sockets when we stop the thread
		connectionSocket.close();
		dataSocket.close();
	});

	// Keep the program running indefinately, while sending messages
	while(true) {
		if(dataSocket.isOpen()) {
			std::time_t now = std::time(nullptr);
			std::string message = std::asctime(std::localtime(&now));
			dataSocket.send(message.c_str(), message.size());
			std::cout << "Sent " << message << "!" << std::endl;

			std::this_thread::sleep_for(100ms);
		}
	}

	return 0;
}