#ifndef __MESSAGE_QUEUE_HPP__
#define __MESSAGE_QUEUE_HPP__

#include <queue>
#include <circular_buffer.hpp>
#include "messages.hpp"

#include "include_everywhere.hpp"

// Singleton responsible for processing and verifying messages
struct MessageManager {
	friend class PeerManager;

	// The the application is mangaing
	std::vector<std::filesystem::path>* folders;

	// Variables tracking how many files we need to recieve before our state is the same as the network
	size_t recievedInitialFiles = 0, totalInitialFiles = 1;

	// Queue of messages waiting to be processed (It is a non-blocking [skiplist based] concurrent queue)
	// NOTE: Lower priorities = faster execution
	using Prio = std::pair<size_t, std::unique_ptr<Message>>;
	struct PrioComp {
		bool operator() (const Prio& a, const Prio& b) {
			return a.first > b.first;
		}
	};
	mutable monitor<std::priority_queue<Prio, std::vector<Prio>, PrioComp>> messageQueue;

	// Circular buffer that maintains a record of the past 100 messages that have been recieved or sent
	finalizeable_circular_buffer_array<std::unique_ptr<Message>, 100> oldMessages;



	// Function which gets the MessageManager singleton
	static MessageManager& singleton() {
		static MessageManager instance;
		return instance;
	}

	// Function which gets a reference to the managed folders, and sets up the circular buffer to free released pointers
	void setup(std::vector<std::filesystem::path>& folders){
		this->folders = &folders;
		oldMessages.setFinalizer([](std::unique_ptr<Message>& m){ m.release(); });
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
		int64_t requeuePriority; // -1 indicates no requeue needed
		switch(msgPtr->type) {
		break; case Message::Type::payload:{
			auto& m = reference_cast<PayloadMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "][payload]:\n" << m.payload << std::endl;
			requeuePriority = -1;
		}
		break; case Message::Type::resendRequest:{
			auto& m = reference_cast<ResendRequestMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] resend request message" <<  std::endl;
			requeuePriority = processResendRequestMessage(m) ? -1 : 0;
		}
		break; case Message::Type::lock:{
			auto& m = reference_cast<FileMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] lock message" << std::endl;
			requeuePriority = processLockMessage(m) ? -1 : 5;
		}
		break; case Message::Type::unlock:{
			auto& m = reference_cast<FileMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] unlock message" << std::endl;
			requeuePriority = processUnlockMessage(m) ? -1 : 5;
		}
		break; case Message::Type::deleteFile:{
			auto& m = reference_cast<FileMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] delete message" << std::endl;
			requeuePriority = processDeleteFileMessage(m) ? -1 : 5;
		}
		break; case Message::Type::create:{
			auto& m = reference_cast<FileContentMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] create message" << std::endl;
			requeuePriority = processCreateFileMessage(m) ? -1 : 5;
		}
		break; case Message::Type::initialSync:{
			auto& m = reference_cast<FileInitialSyncMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] sync message" << std::endl;
			requeuePriority = processInitialFileSyncMessage(m) ? -1 : 4;
		}
		break; case Message::Type::initialSyncRequest:{
			auto& m = reference_cast<Message>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] sync request message" << std::endl;
			requeuePriority = processInitialFileSyncRequestMessage(m) ? -1 : 4;
		}
		break; case Message::Type::change:{
			auto& m = reference_cast<FileChangeMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] change message" << std::endl;
			requeuePriority = processChangeFileMessage(m) ? -1 : 5;
		}
		break; case Message::Type::connect:{
			auto& m = reference_cast<ConnectMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] connect message" << std::endl;
			requeuePriority = processConnectMessage(m) ? -1 : 1;
		}
		break; case Message::Type::disconnect:{
			auto& m = reference_cast<Message>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] disconnect message" << std::endl;
			requeuePriority = processDisconnectMessage(m) ? -1 : 2;
		}
		break; case Message::Type::linkLost:{
			auto& m = reference_cast<Message>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] link-lost message" << std::endl;
			requeuePriority = processLinkLostMessage(m) ? -1 : 0;
		}
		break; default:
			throw std::runtime_error("Unrecognized message type");
		}

		// If the message was successful, move the message into the buffer of old messages
		if(requeuePriority == -1)
			oldMessages.emplace_back(std::move(msgPtr));
		// Otherwise move it back into the queue
		else
			messageQueue->emplace(requeuePriority, std::move(msgPtr));
	}

	// Function that checks to make sure we have finished connecting to the network
	bool isFinishedConnecting() { return recievedInitialFiles == totalInitialFiles; }

private:
	// Only the singleton can be constructed
	MessageManager() {}

	// Validate the provided message, returns true if the hashes match, requests a resend and returns false otherwise
	bool validateMessageHash(const Message& m, uint8_t offset = 0) const;


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

			// Validate message hash
			if(!validateMessageHash(*m))
				return;
			// Payloads have a low priority
			messageQueue->emplace(10, std::move(m));
		}
		break; case Message::Type::resendRequest:{
			auto m = std::make_unique<ResendRequestMessage>();
			ar >> *m;

			// Validate message hash
			if(!validateMessageHash(*m))
				return;
			// Resend requests are processed before anything else
			messageQueue->emplace(0, std::move(m));
		}
		break; case Message::Type::lock:{
			auto m = std::make_unique<FileMessage>();
			ar >> *m;

			// Validate message hash
			if(!validateMessageHash(*m, 1))
				return;
			// File messages have priority 5
			messageQueue->emplace(5, std::move(m));
		}
		break; case Message::Type::unlock:{
			auto m = std::make_unique<FileMessage>();
			ar >> *m;

			// Validate message hash
			if(!validateMessageHash(*m, 1))
				return;
			// File messages have priority 5
			messageQueue->emplace(5, std::move(m));
		}
		break; case Message::Type::deleteFile:{
			auto m = std::make_unique<FileMessage>();
			ar >> *m;

			// Validate message hash
			if(!validateMessageHash(*m, 1))
				return;
			// File messages have priority 5
			messageQueue->emplace(5, std::move(m));
		}
		break; case Message::Type::create:{
			auto m = std::make_unique<FileChangeMessage>();
			ar >> *m;

			// Validate message hash
			if(!validateMessageHash(*m, 1))
				return;
			// File messages have priority 5
			messageQueue->emplace(5, std::move(m));
		}
		break; case Message::Type::initialSync:{
			auto m = std::make_unique<FileInitialSyncMessage>();
			ar >> *m;

			// Validate message hash
			if(!validateMessageHash(*m, 1))
				return;
			// Syncs are executed before other file messages 4
			messageQueue->emplace(4, std::move(m));
		}
		break; case Message::Type::change:{
			auto m = std::make_unique<FileChangeMessage>();
			ar >> *m;

			// Validate message hash
			if(!validateMessageHash(*m, 1))
				return;
			// File messages have priority 5
			messageQueue->emplace(5, std::move(m));
		}
		break; case Message::Type::connect:{
			auto m = std::make_unique<ConnectMessage>();
			ar >> *m;

			// Validate message hash
			if(!validateMessageHash(*m))
				return;
			// Connect has highest priority
			messageQueue->emplace(1, std::move(m));
		}
		break; case Message::Type::disconnect:{
			auto m = std::make_unique<Message>();
			ar >> *m;

			// Validate message hash
			if(!validateMessageHash(*m))
				return;
			// Disconnect is processed after connect
			messageQueue->emplace(2, std::move(m));
		}
		break; default:
			throw std::runtime_error("Unrecognized message type");
		}
	}

	// Functions that process individual types of messages
	// NOTE: They all return true if the message was successfully proccessed and false if the message needs to be readded to the queue for later processing
	bool processResendRequestMessage(const ResendRequestMessage& m);
	bool processLockMessage(const FileMessage& m);
	bool processUnlockMessage(const FileMessage& m);
	bool processDeleteFileMessage(const FileMessage& m);
	bool processCreateFileMessage(const FileContentMessage& m);
	bool processInitialFileSyncMessage(const FileInitialSyncMessage& m);
	bool processInitialFileSyncRequestMessage(const Message& m);
	bool processChangeFileMessage(const FileChangeMessage& m);
	bool processConnectMessage(const ConnectMessage& m);
	bool processLinkLostMessage(const Message& m);
	bool processDisconnectMessage(const Message& m);
};

#endif // __MESSAGE_QUEUE_HPP__