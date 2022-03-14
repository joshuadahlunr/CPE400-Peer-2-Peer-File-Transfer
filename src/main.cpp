#include "ztnode.hpp"

int main() {
	ZeroTierNode node;
	node.setup();

	std::cout << "\nConnection IP: >> " << node.getIP() << " <<\n" << std::endl;

	return 0;
}