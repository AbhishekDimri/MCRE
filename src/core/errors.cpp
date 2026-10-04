#include "mcre/core/errors.hpp"
#include <string>

namespace mcre {
	[[noreturn]] void throwNotImplemented(const char* what, const char* func,
		const char* file, int line) {
		throw NotImplementedError("NOT IMPLEMENTED : " + std::string(what) + " in function " + func +
			" at " + file + ":" + std::to_string(line));
	}
}
