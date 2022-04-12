#ifndef __MESSAGE_QUEUE_HPP__
#define __MESSAGE_QUEUE_HPP__

#include <queue>
#include "messages.hpp"

#include "include_everywhere.hpp"

// Singleton responsible for processing and verifying messages
struct MessageManager {
	friend class PeerManager;

	// The the application is mangaing
	std::vector<std::filesystem::path>* folders;

	// Queue of messages waiting to be processed (It is a non-blocking [skiplist based] concurrent queue)
	// NOTE: Lower priorities = faster execution
	using Prio = std::pair<size_t, std::unique_ptr<Message>>;
	struct PrioComp {
		bool operator() (const Prio& a, const Prio& b) {
			return a.first > b.first;
		}
	};
	mutable monitor<std::priority_queue<Prio, std::vector<Prio>, PrioComp>> messageQueue;

	// Function which gets the MessageManager singleton
	static MessageManager& singleton() {
		static MessageManager instance;
		return instance;
	}

	// Function that processes the next message currently in the message queue
	//	(or waits 1/10 of a second if there is nothing in the queue)
	void processNextMessage(){
		// If the queue is empty, sleep for 100ms
		if(messageQueue->empty()){
			std::this_thread::sleep_for(100ms);
			return;
		}

		// Save the message and remove the node from the queue
		std::unique_ptr<Message> msgPtr = std::move(reference_cast<Prio>(messageQueue->top()).second);
		messageQueue->pop();


		// Process the message as the same type of message that was delivered
		switch(msgPtr->type) {
		break; case Message::Type::payload:{
			auto& m = reference_cast<PayloadMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "][payload]:\n" << m.payload << std::endl;
		}
		break; case Message::Type::lock:{
			auto& m = reference_cast<FileMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] lock message" << std::endl;
			processLockMessage(m);
		}
		break; case Message::Type::unlock:{
			auto& m = reference_cast<FileMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] unlock message" << std::endl;
			processUnlockMessage(m);
		}
		break; case Message::Type::deleteFile:{
			auto& m = reference_cast<FileMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] delete message" << std::endl;
			processDeleteFileMessage(m);
		}
		break; case Message::Type::create:{
			auto& m = reference_cast<FileContentMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] create message" << std::endl;
			processCreateFileMessage(m);
		}
		break; case Message::Type::initialSync:{
			auto& m = reference_cast<FileInitialSyncMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] sync message" << std::endl;
			processInitialFileSyncMessage(m);
		}
		break; case Message::Type::initialSyncRequest:{
			auto& m = reference_cast<Message>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] sync request message" << std::endl;
			processInitialFileSyncRequestMessage(m);
		}
		break; case Message::Type::change:{
			auto& m = reference_cast<FileChangeMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] change message" << std::endl;
			processChangeFileMessage(m);
		}
		break; case Message::Type::connect:{
			auto& m = reference_cast<ConnectMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] connect message" << std::endl;
			processConnectMessage(m);
		}
		break; case Message::Type::disconnect:{
			auto& m = reference_cast<Message>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] disconnect message" << std::endl;
			processDisconnectMessage(m);
		}
		break; case Message::Type::linkLost:{
			auto& m = reference_cast<Message>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] link-lost message" << std::endl;
			processLinkLostMessage(m);
		}
		break; default:
			throw std::runtime_error("Unrecognized message type");
		}
	}

private:
	// Only the singleton can be constructed
	MessageManager() {}

	// Function that deserializes a message received from the network and adds it to the message queue
	void deserializeMessage(const std::span<std::byte> data) const {
		// Extract the type of message
		Message::Type type = (Message::Type) uint8_t(data[10]);
		if((uint8_t)type == 0) type = (Message::Type) uint8_t(data[5]);
		if((uint8_t)type == 0) type = (Message::Type) uint8_t(data[20]);
		// Copy the data into a deserialization buffer
		std::stringstream backing({(char*) data.data(), data.size()});
		boost::archive::binary_iarchive ar(backing, archiveFlags);


		// Deserialize the message as the same type of message that was delivered and add it to the message queue
		switch(type) {
		break; case Message::Type::payload:{
			auto m = std::make_unique<PayloadMessage>();
			ar >> *m;
			// TODO: Check hash
			// std::cout << m->messageHash << " - " << m->hash() + 1 << std::endl;
			// Payloads have a low priority
			messageQueue->emplace(10, std::move(m));
		}
		break; case Message::Type::lock:{
			auto m = std::make_unique<FileMessage>();
			ar >> *m;
			// TODO: Check hash
			// std::cout << m->messageHash << " - " << m->hash() + 1 << std::endl;
			// File messages have priority 5
			messageQueue->emplace(5, std::move(m));
		}
		break; case Message::Type::unlock:{
			auto m = std::make_unique<FileMessage>();
			ar >> *m;
			// TODO: Check hash
			// std::cout << m->messageHash << " - " << m->hash() + 1 << std::endl;
			// File messages have priority 5
			messageQueue->emplace(5, std::move(m));
		}
		break; case Message::Type::deleteFile:{
			auto m = std::make_unique<FileMessage>();
			ar >> *m;
			// TODO: Check hash
			// std::cout << m->messageHash << " - " << m->hash() + 1 << std::endl;
			// File messages have priority 5
			messageQueue->emplace(5, std::move(m));
		}
		break; case Message::Type::create:{
			auto m = std::make_unique<FileChangeMessage>();
			ar >> *m;
			// TODO: Check hash
			// std::cout << m->messageHash << " - " << m->hash() + 1 << std::endl;
			// File messages have priority 5
			messageQueue->emplace(5, std::move(m));
		}
		break; case Message::Type::initialSync:{
			auto m = std::make_unique<FileInitialSyncMessage>();
			ar >> *m;
			// TODO: Check hash
			std::cout << m->messageHash << " - " << m->hash() + 1 << std::endl;
			// Syncs are executed before other file messages 4
			messageQueue->emplace(4, std::move(m));
		}
		break; case Message::Type::change:{
			auto m = std::make_unique<FileChangeMessage>();
			ar >> *m;
			// TODO: Check hash
			// std::cout << m->messageHash << " - " << m->hash() + 1 << std::endl;
			// File messages have priority 5
			messageQueue->emplace(5, std::move(m));
		}
		break; case Message::Type::connect:{
			auto m = std::make_unique<ConnectMessage>();
			ar >> *m;
			// TODO: Check hash
			// std::cout << m->messageHash << " - " << m->hash() + 1 << std::endl;
			// Connect has highest priority
			messageQueue->emplace(1, std::move(m));
		}
		break; case Message::Type::disconnect:{
			auto m = std::make_unique<Message>();
			ar >> *m;
			// TODO: Check hash
			// std::cout << m->messageHash << " - " << m->hash() + 1 << std::endl;
			// Disconnect is processed after disconnect
			messageQueue->emplace(2, std::move(m));
		}
		break; default:
			throw std::runtime_error("Unrecognized message type");
		}
	}

	// Functions that process individual types of messages
	void processLockMessage(const FileMessage& m);
	void processUnlockMessage(const FileMessage& m);
	void processDeleteFileMessage(const FileMessage& m);
	void processCreateFileMessage(const FileContentMessage& m);
	void processInitialFileSyncMessage(const FileInitialSyncMessage& m);
	void processInitialFileSyncRequestMessage(const Message& m);
	void processChangeFileMessage(const FileChangeMessage& m);
	void processConnectMessage(const ConnectMessage& m);
	void processLinkLostMessage(const Message& m);
	void processDisconnectMessage(const Message& m);
};

#endif // __MESSAGE_QUEUE_HPP__