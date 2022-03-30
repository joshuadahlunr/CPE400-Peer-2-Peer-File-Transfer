// This files includes all of the includes and definitions needed everywhere

#ifndef __INCLUDE_EVERYWHERE_HPP__
#define __INCLUDE_EVERYWHERE_HPP__

#include <iostream>
#include <chrono>
#include <thread>
#include <vector>

// Include nonstd::span as std::span
#include <span.hpp>
namespace std { using namespace nonstd; }

using namespace std::literals;

// Converts a reference of one type to another type... no checks are performed, no bits are changed, can cast away const, it is completely unsafe
template<typename Out, typename In>
constexpr inline Out& reference_cast(In& in) { return *((Out*) &in); }


// Converts an arbitrary time point into a system clock time point
template <typename OutTP /* = std::chrono::system_clock::time_point */, typename InTP>
auto convertTimepoint(InTP tp) {
	return std::chrono::time_point_cast<typename OutTP::clock::duration>(tp - InTP::clock::now() + OutTP::clock::now());
}

// Converts a timepoint into a time_t
template <typename TP>
time_t to_time_t(TP tp) {
	return std::chrono::system_clock::to_time_t(convertTimepoint<std::chrono::system_clock::time_point>(tp));
}

#endif // __INCLUDE_EVERYWHERE_HPP__