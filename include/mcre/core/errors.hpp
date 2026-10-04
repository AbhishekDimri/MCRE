#pragma once

#include <stdexcept>
#include <string>

#define MCRE_TODO(what) \
  ::mcre::throwNotImplemented((what), __func__, __FILE__, __LINE__)

namespace mcre {
	class NotImplementedError : public std::logic_error {
	public:
		explicit NotImplementedError(const std::string& what) : std::logic_error(what) {}
	};

	[[noreturn]] void throwNotImplemented(const char* what, const char* func,
		const char* file, int line);
}