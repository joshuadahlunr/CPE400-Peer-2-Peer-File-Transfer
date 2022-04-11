#ifndef __MESSAGE_QUEUE_HPP__
#define __MESSAGE_QUEUE_HPP__

#include <lockfree_skiplist_priority_queue.h>
#include "messages.hpp"

#include "include_everywhere.hpp"

// Singleton responsible for processing and verifying messages
struct MessageManager {
	friend class PeerManager;

	// Queue of messages waiting to be processed (It is a non-blocking [skiplist based] concurrent queue)
	// NOTE: Lower priorities = faster execution
	mutable skipListQueue<std::unique_ptr<Message>> messageQueue;

	// Function which gets the MessageManager singleton
	static MessageManager& singleton() {
		static MessageManager instance;
		return instance;
	}

	// Function that processes the next message currently in the message queue
	//	(or waits 1/10 of a second if there is nothing in the queue)
	void processNextMessage();

private:
	// Only the singleton can be constructed
	MessageManager() {}

	// Function that deserializes a message received from the network and adds it to the message queue
	void deserializeMessage(const std::span<std::byte> data) const {
		// Extract the type of message
		Message::Type type = (Message::Type) uint8_t(data[10]);
		if((uint8_t)type == 0) type = (Message::Type) uint8_t(data[5]);
		// Copy the data into a deserialization buffer
		std::stringstream backing({(char*) data.data(), data.size()});
		boost::archive::binary_iarchive ar(backing, archiveFlags);


		// Deserialize the message as the same type of message that was delivered and add it to the message queue
		switch(type) {
		break; case Message::Type::payload:{
			auto m = std::make_unique<PayloadMessage>();
			ar >> *m;
			// TODO: Check hash
			// Payloads have a low priority
			messageQueue.insert(10, std::move(m));
		}
		break; case Message::Type::lock:{
			auto m = std::make_unique<FileMessage>();
			ar >> *m;
			// TODO: Check hash
			// File messages have priority 5
			messageQueue.insert(5, std::move(m));
		}
		break; case Message::Type::unlock:{
			auto m = std::make_unique<FileMessage>();
			ar >> *m;
			// TODO: Check hash
			// File messages have priority 5
			messageQueue.insert(5, std::move(m));
		}
		break; case Message::Type::deleteFile:{
			auto m = std::make_unique<FileMessage>();
			ar >> *m;
			// TODO: Check hash
			// File messages have priority 5
			messageQueue.insert(5, std::move(m));
		}
		break; case Message::Type::create:{
			auto m = std::make_unique<FileCreateMessage>();
			ar >> *m;
			// TODO: Check hash
			// File messages have priority 5
			messageQueue.insert(5, std::move(m));
		}
		break; case Message::Type::change:{
			auto m = std::make_unique<FileChangeMessage>();
			ar >> *m;
			// TODO: Check hash
			// File messages have priority 5
			messageQueue.insert(5, std::move(m));
		}
		break; case Message::Type::connect:{
			auto m = std::make_unique<ConnectMessage>();
			ar >> *m;
			// TODO: Check hash
			// Connect has highest priority
			messageQueue.insert(1, std::move(m));
		}
		break; case Message::Type::disconnect:{
			auto m = std::make_unique<Message>();
			ar >> *m;
			// TODO: Check hash
			// Disconnect is processed after disconnect
			messageQueue.insert(2, std::move(m));
		}
		break; default:
			throw std::runtime_error("Unrecognized message type");
		}
	}
};

#endif // __MESSAGE_QUEUE_HPP__