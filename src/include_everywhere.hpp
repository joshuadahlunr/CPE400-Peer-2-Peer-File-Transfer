// This files includes all of the includes and definitions needed everywhere

#ifndef __INCLUDE_EVERYWHERE_HPP__
#define __INCLUDE_EVERYWHERE_HPP__

#include <iostream>
#include <chrono>
#include <thread>

using namespace std::literals;

// Converts a reference of one type to another type... no checks are performed, no bits are changed, can cast away const, it is completely unsafe
template<typename Out, typename In>
constexpr inline Out& reference_cast(In& in) { return * (Out*) &in; }

#endif // __INCLUDE_EVERYWHERE_HPP__