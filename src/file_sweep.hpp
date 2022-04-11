#ifndef __FILE_SWEEP_HPP__
#define __FILE_SWEEP_HPP__

#include <map>
#include <vector>
#include <boost/predef.h>

#include "include_everywhere.hpp"

// Class which sweeps the provided folder structure every time its sweep function is called
// There is a fast track optimization, where a recently modified subset of files is sweept every call, or a total sweep scanning all of the folders can be preformed
struct FilesystemSweeper {
	// Reference to the folders the system is responsible for sweeping
	const std::vector<std::filesystem::path>& folders;

	using PathCallback = void(*)(const std::filesystem::path& path);
	// Function callback (return void, taking path) called when the sweeper detects that a file has been created
	PathCallback onFileCreated,
	// Function callback (return void, taking path) called when the sweeper detects that a file has been modified
		onFileModified,
	// Function callback (return void, taking path) called when the sweeper detects that a file has been deleted
		onFileDeleted;

	// Timestamps and counters tracking the last time every file was modified
	std::map<std::filesystem::path, std::pair<std::filesystem::file_time_type, size_t>> timestamps;
	// Timestamps and counters tracking when recently modified files were modified
	std::map<std::filesystem::path, std::pair<std::filesystem::file_time_type, size_t>> fastTrackTimestamps;
	// Counter used to detect deleted files (if we update the counters of all of the scanned files,
	//	but a file we are tracking doesn't get its counter updated, that means it was deleted)
	size_t iteration = 0;

	// Function which sets up the file sweaper
	void setup() {
		// Remove all of the .wnts folders
		for(auto& folder: folders)
			remove_all(folder / ".wnts");

		// Paths to the files this sweep should scan
		std::vector<std::filesystem::path> paths = enumerateAllFiles(folders);

		// Copy all of the files into the .wnts folder
		for(auto& path: paths) {
			auto wnts = wntsPath(path);
			create_directories(wnts.remove_filename());
			copy(path, wnts, std::filesystem::copy_options::update_existing);
		}
	}

	// Function which calls sweep, automatically preforming a total sweep every <n> iterations
	// Behind the scenes it scans the file system and reports (via callback functions) all of the changed, modified, and deleted files
	void totalSweepEveryN(size_t n) {
		sweep(iteration % n == 0);
	}

	// Function which scans the file system and reports (via callback functions) all of the changed, modified, and deleted files
	void sweep(bool total = false) {
		// Paths to the files this sweep should scan
		std::vector<std::filesystem::path> paths;
		// Pointer to the appropriate timestamp map (default total sweep map)
		auto* timestamps = &this->timestamps;

		// If we are doing a total sweep, recursively add all of the files to <paths> (except those in the .wnts folder)
		if(total)
			enumerateAllFiles(folders, paths);
		// If we are doing a fasttrack sweep, update the timestamp map point and add all of the current fasttrack files to <paths>
		else {
			for (auto& [path, _]: fastTrackTimestamps)
				paths.push_back(path);
			timestamps = &fastTrackTimestamps;
		}

		// Variables tracking paths that have been deleted or that should be removed from the fast track (haven't been modiifed recently)
		std::vector<std::filesystem::path> removedFiles, fastTrackRemovedFiles;

		// For every file this scan should consider...
		try {
			for(auto& path: paths) {
				// Determine the timestamp of when the file was last modified
				auto timestamp = last_write_time(path);
				auto pair = std::make_pair(timestamp, iteration);

				// If we aren't tracking this file, that means it was created
				if(timestamps->find(path) == timestamps->end()){
					// File has been created!
					onFileCreated(path);

					fastTrackTimestamps[path] = pair; // Mark the file as being fast tracked
				// If our stored timestamp for this file is older than its most recent timestamp it has been modified
				} else if((*timestamps)[path].first < timestamp){
					// File has been modified!
					onFileModified(path);

					fastTrackTimestamps[path] = pair; // Mark the file as being fast tracked
				}

				// Update timestamp and sweep iteration information for this file
				(*timestamps)[path] = pair;
			}
		// If we are trying to get the timestamp for a file which no longer exists, mark that file as deleted
		} catch(std::filesystem::filesystem_error e) {
			if(e.code().value() == 2)
				removedFiles.push_back(e.path1());
			else throw e;
		}

		// Calculate the current time
		auto now = std::filesystem::file_time_type::clock::now();

		// For every tracked file...
		for(auto& [path, pair]: *timestamps){
			auto& [timestamp, sweepIteration] = pair;

			// If its sweep iteration doesn't match the current sweep iteration, the file has been deleted
			if(sweepIteration != iteration){
				// File has been deleted!
				onFileDeleted(path);

				removedFiles.emplace_back(path); // Mark the file as deleted
			// If the file hasn't been modified recently (within 10 seconds), the file is no longer fast tracked
			} else if(std::chrono::duration_cast<std::chrono::milliseconds>(now - timestamp).count() > 10'000)
				fastTrackRemovedFiles.emplace_back(path);

		}

		// Remove deleted files from both the total and fast track maps
		for(auto& path: removedFiles) {
			this->timestamps.erase(path);
			fastTrackTimestamps.erase(path);
		}

		// Remove unfast-tracked files from the fast track map
		for(auto& path: fastTrackRemovedFiles)
			fastTrackTimestamps.erase(path);

		iteration++;
	}
};

#endif // __FILE_SWEEP_HPP__