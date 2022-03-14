// This file includes all of the includes and definitions needed for networking

#ifndef __NETWORKING_HPP__
#define __NETWORKING_HPP__

#include "include_everywhere.hpp"
#include <ZTCpp.hpp>

namespace zt = jbatnozic::ztcpp;


// Path to the ZeroTier node's identity
constexpr const char* ztIdentityPath = ".nodedata";
// Network ID of the ZeroTier network for this application
constexpr uint64_t ztNetworkID = 0x6ab565387ae649e4;
// Port number that ZeroTier uses
constexpr uint16_t ztServicePort = 9994;

#endif // __NETWORKING_HPP__