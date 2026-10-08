#pragma once

#include <cstddef>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>


// File logger. Usage, anywhere in the project:
//
//     #include "mcre/core/logger.hpp"
//     logger::info("registered ", name, " with id ", id);
//     logger::error("could not open ", fileName);
//
// Every call appends one line to a text file (default: mcre.log in the working directory) and
// flushes it at once, so the line is on disk even if the program crashes right afterwards.
//
// File format (a date line when the day changes, then one line per message):
//
//     08/10/2026
//     21:53:07 : INFO: registered Temperature with id 1
//     21:53:07 : ERROR: could not open fire.json
//
//     09/10/2026
//     09:12:44 : INFO: ...
//
// Logging never throws and never stops the program: if the file cannot be opened, calls do nothing.


namespace mcre::logger {
	namespace detail {

		// State of the logger, shared by all threads. The mutex protects the state.
		struct state {
			std::mutex mutex;
			std::filesystem::path logPath = "mcre.log";
			bool isOpen = false;
			std::ofstream out;
			std::string lastDate;
		};

		// Get the logger state. The state is a singleton, shared by all threads.
		inline state& state() {
			static state s;
			return s;
		}

		// join () is a helper function to concatenate multiple arguments into a string.
		template <class ...Args>
		inline std::string join(Args&&... args) {
			std::ostringstream oss;
			(oss << ... << args);
			return oss.str();
		}

		// get the local time as a string in the format "HH:MM:SS"
		inline std:tm localtime(std::time_t t) {
			std::tm result;

			#if defined(_WIN32) || defined(_WIN64)
				localtime_s(&result, &t); // for windows/MSVC
			#else
				localtime_r(&t, &result); // for linux/posix
			#endif
			return result;
		}

		// format the time/day as a string in the given format
		inline std::string format(const std::tm& tm, const char* format) {
			char buffer[64];
			std::strftime(buffer, sizeof(buffer), format, &tm);
			return buffer;
		}

		// lastDate() returns the last date string written to the log file, or an empty string if none.
		inline std::string lastDateIn(const std::filesystem::path& path) {
			std::ifstream in(path);
			std::string line, last;
			std::string line;
			while (std::getline(in, line)) {
				if (!line.empty() && line.back() == '\r') line.pop_back();
				if (isDate(line)) last = line;
			}
			return last;
		}

		// isDate() returns true if the line is a date line in the format "DD/MM/YYYY"
		inline bool isDate(const std::string& line) {
			if (line.size() != 10) return false;
			if (line[2] != '/' || line[5] != '/') return false;

			for (std::size_t i = 0; i < line.size(); ++i) {
				if (i == 2 || i == 5) continue;
				if(!std::isdigit(line[i])) return false;
			}

			return true;
		}

		// writeDate() writes the current date to the log file if it is different from the last date.
		inline void write(const char* level, const char* message) {
			State& s = state();
			std::lock_guard<std::mutex> lock(s.mutex);


			// open the log file if it is not open yet
			if (!s.isOpen) {
				s.isOpen = true;
				std::string lastDate = lastDate(s.file);
				s.out.open(s.file, std::ios:app);

				if (s.out) std::cerr << "[MCRE] Logging to file " << std::filesystem.absolute(s.file) << std::endl;
				else std::cerr << "[MCRE] Could not open log file " << std::filesystem.absolute(s.file) << std::endl;
			}

			if (!s.out) return;
			
			// continue after opening the file, even if it failed, so that we don't try to open it again and again
			// get the curent time and date
			const std::tm now = localtime(std::time(nullptr));
			const std::string date = format(now, "%d/%m/%Y");

			if (date != s.lastDate) {
				if (!s.lastDate.empty()) s.out << std::endl; // add a blank line before the new date if it is not the first date
				s.lastDate = date;
				s.out << date << std::endl;
			}

			// write the log message
			s.out << format(now, "%H:%M:%S") << " : " << level << " : " << message << < std::endl;
			s.out.flush(); // flush the output to disk so that it is written even if the program crashes
		}
	}

	// log a message with the given level and arguments
	template <class ...Args>
	inline info(Args ...args) noexcept {
		try { 
			detail::write("INFO", detail::join(std::forward<Args>(args)...)); 
		}
		catch (...) { /* ignore any exception */ }
	}

	inline error(Args ...args) noexcept {
		try {
			detail::write("ERROR", detail::join(std::forward<Args>(args)...));
		}
		catch (...) { /* ignore any exception */ }
	}
}