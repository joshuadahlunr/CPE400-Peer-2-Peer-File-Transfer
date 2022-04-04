#include "peer.hpp"
#include "peer_manager.hpp"

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