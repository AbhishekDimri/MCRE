# Defines the engine as a static library named "mcre".
# A static library is a bundle of compiled code that other targets (the app,
# and later the tests) link against, so the engine is compiled only once.

add_library(mcre STATIC
    # Engine source files. Paths are relative to this folder (src/).
    # Every new .cpp you write must be added to this list, otherwise it is
    # never compiled and you get "unresolved external symbol" at link time.
    core/errors.cpp

    # Headers are listed only so Visual Studio associates them with this
    # target (gives IntelliSense and the project dropdown). The compiler does
    # not need them here; it finds headers via the include directory below.
    ${PROJECT_SOURCE_DIR}/include/mcre/core/errors.hpp
)

# Tells the compiler where to search for headers. PROJECT_SOURCE_DIR is the
# folder holding the root CMakeLists.txt, so this works wherever this file sits.
# Because the folder is "include", code writes #include "mcre/core/errors.hpp".
# PUBLIC means this setting is passed on to every target that links to mcre
# (the app and tests), so they do not need to repeat it.
target_include_directories(mcre PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

# Requires C++20. CMake translates this into the right compiler flag
# (/std:c++20 for MSVC, -std=c++20 for GCC/Clang). PUBLIC so everything that
# links to mcre also compiles with at least C++20.
target_compile_features(mcre PUBLIC cxx_std_20)

# Compiler warnings, which act as free bug reports. The flags differ by compiler.
# PRIVATE: these apply only to the engine's own code, not to targets that link
# to it.
if (MSVC)
    # /W4 = high warning level, /permissive- = strict standard-conforming mode
    target_compile_options(mcre PRIVATE /W4 /permissive-)
else()
    # GCC and Clang equivalents
    target_compile_options(mcre PRIVATE -Wall -Wextra -Wpedantic)
endif()