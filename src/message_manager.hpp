#ifndef __MESSAGE_QUEUE_HPP__
#define __MESSAGE_QUEUE_HPP__

#include <queue>
#include <circular_buffer.hpp>
#include "messages.hpp"

#include "include_everywhere.hpp"

// Singleton responsible for processing and verifying messages
struct MessageManager {
	friend class PeerManager;

	// Priorities of different types of messages (lower = faster)
	static constexpr auto payloadPriority = 10;
	static constexpr auto resendPriority = 0;
	static constexpr auto lockPriority = 4;
	static constexpr auto filePriority = 5;
	static constexpr auto connectPriority = 1;
	static constexpr auto disconnectPriority = 2;



	// The the application is mangaing
	std::vector<std::filesystem::path>* folders;

	// Variables tracking how many files we need to recieve before our state is the same as the network
	size_t recievedInitialFiles = 0, totalInitialFiles = 1;

	// Queue of messages waiting to be processed (It is a non-blocking [skiplist based] concurrent queue)
	// NOTE: Lower priorities = faster execution
	using Prio = std::pair<size_t, std::unique_ptr<Message>>;
	struct PrioComp {
		bool operator() (const Prio& a, const Prio& b) {
			// If the two messages have the same priority, and are file messages, sort them according to their timestamps
			if(a.first == b.first) {
				constexpr auto fileTypes = Message::Type::lock | Message::Type::unlock | Message::Type::deleteFile | Message::Type::contentChange | Message::Type::initialSync | Message::Type::change;
				if(a.second->type & fileTypes && b.second->type & fileTypes)
					return std::chrono::duration_cast<std::chrono::nanoseconds>(
						reference_cast<FileMessage>(*a.second).timestamp - reference_cast<FileMessage>(*b.second).timestamp
					).count() < 0; // If a should come first, its timestamp will be smaller and thus the difference will be negative
			}
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
	void setup(std::vector<std::filesystem::path>& folders) {
		this->folders = &folders;
		oldMessages.setFinalizer([](std::unique_ptr<Message>& m){ m.release(); });
	}


	// Function that processes the next message currently in the message queue
	//	(or waits 1/10 of a second if there is nothing in the queue)
	void processNextMessage() {
		// If the queue is empty, sleep for 100ms
		if(messageQueue->empty()) {
			std::this_thread::sleep_for(100ms);
			return;
		}

		// Save the message and remove the node from the queue
		std::unique_ptr<Message> msgPtr = std::move(reference_cast<Prio>(messageQueue->top()).second);
		messageQueue->pop();


		// Process the message as the same type of message that was delivered
		int64_t requeuePriority; // -1 indicates no requeue needed
		switch(msgPtr->type) {
		break; case Message::Type::payload: {
			auto& m = reference_cast<PayloadMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "][payload]:\n" << m.payload << std::endl;
			requeuePriority = -1;
		}
		break; case Message::Type::resendRequest: {
			auto& m = reference_cast<ResendRequestMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] resend request message" <<  std::endl;
			requeuePriority = processResendRequestMessage(m) ? -1 : resendPriority + 1;
		}
		break; case Message::Type::lock: {
			auto& m = reference_cast<FileMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] lock " << m.targetFile << std::endl;
			requeuePriority = processLockMessage(m) ? -1 : lockPriority + 1;
		}
		break; case Message::Type::unlock: {
			auto& m = reference_cast<FileMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] unlock " << m.targetFile << std::endl;
			requeuePriority = processUnlockMessage(m) ? -1 : lockPriority + 1;
		}
		break; case Message::Type::deleteFile: {
			auto& m = reference_cast<FileMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] delete " << m.targetFile << std::endl;
			requeuePriority = processDeleteFileMessage(m) ? -1 : filePriority + 1;
		}
		break; case Message::Type::contentChange: {
			auto& m = reference_cast<FileContentMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] create " << m.targetFile << std::endl;
			requeuePriority = processContentFileMessage(m) ? -1 : filePriority + 1;
		}
		break; case Message::Type::initialSync: {
			auto& m = reference_cast<FileInitialSyncMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] sync " << m.targetFile << std::endl;
			requeuePriority = processInitialFileSyncMessage(m) ? -1 : lockPriority + 1;
		}
		break; case Message::Type::initialSyncRequest: {
			auto& m = reference_cast<Message>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] sync request message" << std::endl;
			requeuePriority = processInitialFileSyncRequestMessage(m) ? -1 : lockPriority + 1;
		}
		break; case Message::Type::change: {
			auto& m = reference_cast<FileChangeMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] change " << m.targetFile << std::endl;
			requeuePriority = processChangeFileMessage(m) ? -1 : filePriority + 1;
		}
		break; case Message::Type::connect: {
			auto& m = reference_cast<ConnectMessage>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] connect message" << std::endl;
			requeuePriority = processConnectMessage(m) ? -1 : connectPriority + 1;
		}
		break; case Message::Type::disconnect: {
			auto& m = reference_cast<Message>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] disconnect message" << std::endl;
			requeuePriority = processDisconnectMessage(m) ? -1 : disconnectPriority + 1;
		}
		break; case Message::Type::linkLost: {
			auto& m = reference_cast<Message>(*msgPtr);
			std::cout << "[" << m.originatorNode << "] link-lost message" << std::endl;
			requeuePriority = processLinkLostMessage(m) ? -1 : resendPriority + 1;
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
		Message::Type type = (Message::Type) uint8_t(data[5]);
		if((uint8_t)type == 0) type = (Message::Type) uint8_t(data[10]);
		if((uint8_t)type == 0) type = (Message::Type) uint8_t(data[15]);
		if((uint8_t)type == 0) type = (Message::Type) uint8_t(data[20]);
		// Copy the data into a deserialization buffer
		std::stringstream backing({(char*) data.data(), data.size()});
		boost::archive::binary_iarchive ar(backing, archiveFlags);


		// Deserialize the message as the same type of message that was delivered and add it to the message queue
		switch(type) {
		break; case Message::Type::payload: {
			auto m = std::make_unique<PayloadMessage>();
			ar >> *m;

			// Validate message hash
			if(!validateMessageHash(*m))
				return;
			// Payloads have a low priority
			messageQueue->emplace(payloadPriority, std::move(m));
		}
		break; case Message::Type::resendRequest: {
			auto m = std::make_unique<ResendRequestMessage>();
			ar >> *m;

			// Validate message hash
			if(!validateMessageHash(*m))
				return;
			// Resend requests are processed before anything else
			messageQueue->emplace(resendPriority, std::move(m));
		}
		break; case Message::Type::lock: {
			auto m = std::make_unique<FileMessage>();
			ar >> *m;

			// Validate message hash
			if(!validateMessageHash(*m, 1))
				return;
			// File messages have priority 5
			messageQueue->emplace(lockPriority, std::move(m));
		}
		break; case Message::Type::unlock: {
			auto m = std::make_unique<FileMessage>();
			ar >> *m;

			// Validate message hash
			if(!validateMessageHash(*m, 1))
				return;
			// File messages have priority 5
			messageQueue->emplace(lockPriority, std::move(m));
		}
		break; case Message::Type::deleteFile: {
			auto m = std::make_unique<FileMessage>();
			ar >> *m;

			// Validate message hash
			if(!validateMessageHash(*m, 1))
				return;
			// File messages have priority 5
			messageQueue->emplace(filePriority, std::move(m));
		}
		break; case Message::Type::contentChange: {
			auto m = std::make_unique<FileContentMessage>();
			ar >> *m;

			// Validate message hash
			if(!validateMessageHash(*m, 1))
				return;
			// File messages have priority 5
			messageQueue->emplace(filePriority, std::move(m));
		}
		break; case Message::Type::initialSync: {
			auto m = std::make_unique<FileInitialSyncMessage>();
			ar >> *m;

			// Validate message hash
			if(!validateMessageHash(*m, 1))
				return;
			// Syncs are executed before other file messages 4
			messageQueue->emplace(lockPriority, std::move(m));
		}
		break; case Message::Type::change: {
			auto m = std::make_unique<FileChangeMessage>();
			ar >> *m;

			// Validate message hash
			if(!validateMessageHash(*m, 1))
				return;
			// File messages have priority 5
			messageQueue->emplace(filePriority, std::move(m));
		}
		break; case Message::Type::connect: {
			auto m = std::make_unique<ConnectMessage>();
			ar >> *m;

			// Validate message hash
			if(!validateMessageHash(*m))
				return;
			// Connect has highest priority
			messageQueue->emplace(connectPriority, std::move(m));
		}
		break; case Message::Type::disconnect: {
			auto m = std::make_unique<Message>();
			ar >> *m;

			// Validate message hash
			if(!validateMessageHash(*m))
				return;
			// Disconnect is processed after connect
			messageQueue->emplace(disconnectPriority, std::move(m));
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
	bool processContentFileMessage(const FileContentMessage& m);
	bool processInitialFileSyncMessage(const FileInitialSyncMessage& m);
	bool processInitialFileSyncRequestMessage(const Message& m);
	bool processChangeFileMessage(const FileChangeMessage& m);
	bool processConnectMessage(const ConnectMessage& m);
	bool processLinkLostMessage(const Message& m);
	bool processDisconnectMessage(const Message& m);
};

#endif // __MESSAGE_QUEUE_HPP__