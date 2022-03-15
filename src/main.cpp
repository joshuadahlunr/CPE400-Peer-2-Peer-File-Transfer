#include "ztnode.hpp"
#include <csignal>

// Variable storing our connection to ZeroTier
ZeroTierNode node;

// Callback that shuts down the program when interupted (ctrl + c in terminal)
void signalCallbackHandler(int signum) {
	// Terminate program
	std::exit(signum);
}

int main() {
	// Gracefully terminate when interupted
	signal(SIGINT, signalCallbackHandler);

	// Establish our connection to ZeroTier
	node.setup();

	std::cout << "\nConnection IP: >> " << node.getIP() << " <<\n" << std::endl;

	return 0;
}