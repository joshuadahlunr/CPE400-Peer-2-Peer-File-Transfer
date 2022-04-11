// This files includes all of the includes and definitions needed everywhere

#ifndef __INCLUDE_EVERYWHERE_HPP__
#define __INCLUDE_EVERYWHERE_HPP__

#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <filesystem>

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


// Function which calculates the same path but in the .wnts folder
inline std::filesystem::path wntsPath(const std::filesystem::path& path){
	auto i = path.begin();
	std::filesystem::path wntsPath = *i++;
	wntsPath /= ".wnts";
	for(; i != path.end(); i++)
		wntsPath /= *i;
	return wntsPath;
}

// Function which fills an array with paths to all of the files we are responsible for sweaping.
static void enumerateAllFiles(const std::vector<std::filesystem::path>& folders, std::vector<std::filesystem::path>& paths) {
	// Recursively add all files in the managed folders
	for(auto& path: folders)
		for (std::filesystem::recursive_directory_iterator i(path), end; i != end; ++i)
			if (!is_directory(i->path())) {
				// Ignore any paths containing .wnts in their folder tree
				bool good = true;
				for(auto folder: i->path())
					if(folder.string() == ".wnts"){
						good = false;
						break;
					}
				if(good) paths.push_back(i->path());
			}
}
inline std::vector<std::filesystem::path> enumerateAllFiles(const std::vector<std::filesystem::path>& folders) {
	std::vector<std::filesystem::path> paths;
	enumerateAllFiles(folders, paths);
	return paths;
}

#endif // __INCLUDE_EVERYWHERE_HPP__