#include "peer.hpp"
#include "peer_manager.hpp"

// Function run by the Peer's thread
void Peer::threadFunction(std::stop_token stop){
	// Variable tracking how much data we should expect to receive in this message
	uint64_t dataSize = 0;
	// Variable tracking how much data we have currently received
	uint64_t dataReceived = 0;

	// Loop unil the thread is requested to stop
	while(!stop.stop_requested()){
		try {
			// std::cout << "threadfn:" << socket._impl.get() << std::endl;

			// Wait upto 100ms for data
			auto pollres = socket.pollEvents(zt::PollEventBitmask::ReadyToReceiveAny, 100ms);
			ZTCPP_THROW_ON_ERROR(pollres, ZTError);

			// If there is data ready to be received...
			if((*pollres & zt::PollEventBitmask::ReadyToReceiveAny) != 0) {
				// Get a pointer to the buffer's memory
				std::byte* bufferMem = &buffer[0];

				// If we haven't determined how much data we have to receive...
				if(dataSize == 0){
					// Read a uint64 worth of data (remember it may take multiple loop iterations to receive that data)
					auto res = socket.receive(bufferMem + dataReceived, sizeof(dataSize) - dataReceived);
					ZTCPP_THROW_ON_ERROR(res, ZTError);
					dataReceived += *res;

					// Once we have read the uint64...
					if(dataReceived >= sizeof(dataSize)){
						// Mark it is as our data size
						dataSize = *((uint64_t*) bufferMem);
						dataReceived -= sizeof(dataSize); // Subtracting to account for the possibility of extra data

						// If there is extra data in the buffer, move it to the front and resize the buffer to match our data
						if(dataReceived > 0) memmove(bufferMem, bufferMem + sizeof(dataSize), buffer.size() - sizeof(dataSize)); // TODO: Needed?
						buffer.resize(dataSize); // TODO: should this only be allowed to grow the buffer?
					}
				// If we have determined how much data we have to receive...
				} else {
					// Read <dataSize> bytes of data (remember it may take multiple loop iterations to receive that data)
					auto res = socket.receive(bufferMem + dataReceived, buffer.size() - dataReceived);
					ZTCPP_THROW_ON_ERROR(res, ZTError);
					dataReceived += *res;

					// Once we have read <dataSize> bytes...
					if(dataReceived >= dataSize) {
						// Process the message
						processMessage({buffer.data(), dataSize});

						// If there is extra data in the buffer, move it to the front
						if(dataReceived > 0) memmove(bufferMem, bufferMem + dataSize, buffer.size() - dataSize); // TODO: Needed?

						// Reset our data size back to 0 (we need to get the size of the next message from the network)
						dataReceived -= dataSize; // Subtracting to account for the possibility of extra data
						dataSize = 0;
					}
				}
			}
		} catch(ZTError e) {
			std::string error = e.what();
			if(error.find("zts_errno=107") != std::string::npos
				| error.find("zts_poll returned ZTS_POLLERR") != std::string::npos)
			{
				// We have been disconnected and this peer is no longer valid
				// Create a new message indicating that our connection to the Peer has been severed
				auto m = std::make_unique<Message>();
				m->type = Message::Type::linkLost;
				m->originatorNode = getRemoteIP();
				MessageManager::singleton().messageQueue->emplace(1, std::move(m)); // Same priority as disconnect messages

				return;
			}

			// TODO: This is where we would need to handle a loss of connection
			std::cerr << "[ZT][Error] " << error << std::endl;
		}
	}
}


// Function that processes a message
void Peer::processMessage(std::span<std::byte> data) {
	// std::cout << "Received " << data.size() << " bytes:\n" << (char*) data.data() << std::endl;

	// Deserialize the root message "header" containing routing data
	std::stringstream backing({(char*) data.data(), sizeof(Message)});
	boost::archive::binary_iarchive ar(backing, boost::archive::no_header);
	Message m;
	ar >> m;
	// If we don't know who sent this bit of data, assume it came from the connected peer
	if(m.senderNode == zt::IpAddress::ipv6Unspecified()) m.senderNode = getRemoteIP();

	// Route the data
	PeerManager::singleton().routeData(data, m.receiverNode, m.senderNode);
}