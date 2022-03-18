#ifndef __NODE_HPP__
#define __NODE_HPP__

#include "networking_include_everywhere.hpp"

class ZeroTierNode: public zt::EventHandlerInterface {
	// Variable tracking if this node is started or not
	bool online = false;
	// Variable tracking the number of networks joined, used to track wether or not we have successfully joined the ZeroTier subnet for this service
	uint8_t networksJoinedCount = 0;
	// Variable tracking the IP address of this node as seen by ZeroTier
	zt::IpAddress ip = zt::IpAddress::ipv4Unspecified();

public:
	// Function which gets the ZeroTierNode singleton	
	static ZeroTierNode& singleton() {
		static ZeroTierNode instance;
		return instance;
	}

	// Function which establishes a connection to ZeroTier
	void setup() {
		ZTCPP_THROW_ON_ERROR(zt::Config::setIdentityFromStorage(ztIdentityPath), ZTError);
		ZTCPP_THROW_ON_ERROR(zt::Config::allowNetworkCaching(true), ZTError);
		ZTCPP_THROW_ON_ERROR(zt::Config::allowPeerCaching(true), ZTError);
		ZTCPP_THROW_ON_ERROR(zt::Config::allowIdentityCaching(true), ZTError);
		ZTCPP_THROW_ON_ERROR(zt::Config::setPort(ztServicePort), ZTError);

		std::cout << "Starting ZeroTier service..." << std::endl;
		zt::LocalNode::setEventHandler(this);
		ZTCPP_THROW_ON_ERROR(zt::LocalNode::start(), ZTError);

		std::cout << "Waiting for node to come online..." << std::endl;
		while (!online)
			std::this_thread::sleep_for(100ms);

		ZTCPP_THROW_ON_ERROR(zt::Network::join(ztNetworkID), ZTError);

		std::cout << "Waiting to join network..." << std::endl;
		while (networksJoinedCount <= 0)
			std::this_thread::sleep_for(1s);
		
		std::cout << "ZeroTier service started!" << std::endl;
	}

	// Disconnect from ZeroTier on shutdown
	~ZeroTierNode() {
		zt::LocalNode::stop();
		// std::this_thread::sleep_for(100ms); // TODO: Is this needed?
		zt::LocalNode::setEventHandler(nullptr);
		std::cout << "ZeroTier service terminated" << std::endl;
	}

	bool isOnline() const { return online; }
	const zt::IpAddress& getIP() const { return ip; }


private:
	// Only the singleton can be constructed
	ZeroTierNode() {}

public:


	// -- ZeroTier Event Handlers --


	void onAddressEvent(zt::EventCode::Address code, const zt::AddressDetails* details) noexcept override {
		std::cout << "[ZT] " << zt::EventDescription(code, details) << std::endl;

		if (details && (code == zt::EventCode::Address::AddedIPv4 || code == zt::EventCode::Address::AddedIPv6))
			ip = details->getIpAddress();
	}

	void onNetworkEvent(zt::EventCode::Network code, const zt::NetworkDetails* details) noexcept override {
		std::cout << "[ZT] " << zt::EventDescription(code, details) << std::endl;

		if (code == zt::EventCode::Network::ReadyIPv4 || 
				code == zt::EventCode::Network::ReadyIPv6 ||
				code == zt::EventCode::Network::ReadyIPv4_IPv6) {
			networksJoinedCount += 1;
		} else if (code == zt::EventCode::Network::Down) 
			networksJoinedCount -= 1;
	}

	void onNetworkInterfaceEvent(zt::EventCode::NetworkInterface code, const zt::NetworkInterfaceDetails* details) noexcept override {
		std::cout << "[ZT] " << zt::EventDescription(code, details) << std::endl;
	}

	void onNetworkStackEvent(zt::EventCode::NetworkStack code, const zt::NetworkStackDetails* details) noexcept override {
		std::cout << "[ZT] " << zt::EventDescription(code, details) << std::endl;
	}

	void onNodeEvent(zt::EventCode::Node code, const zt::NodeDetails* details) noexcept override {
		std::cout << "[ZT] " << zt::EventDescription(code, details) << std::endl;

		if (code == zt::EventCode::Node::Online)
			online = true;
		else if (code == zt::EventCode::Node::Offline)
			online = false;
	}

	void onPeerEvent(zt::EventCode::Peer code, const zt::PeerDetails* details) noexcept override {
		std::cout << "[ZT] " << zt::EventDescription(code, details) << std::endl;
	}

	void onRouteEvent(zt::EventCode::Route code, const zt::RouteDetails* details) noexcept override {
		std::cout << "[ZT] " << zt::EventDescription(code, details) << std::endl;
	}

	void onUnknownEvent(int16_t rawEventCode) noexcept override {
		std::cout << "[ZT] " << "An unknown Zero Tier event was dispatched (" << rawEventCode << ")" << std::endl;
	}
};

#endif // __NODE_HPP__
