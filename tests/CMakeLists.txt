# Unit tests (GoogleTest). Run them from Visual Studio's Test Explorer, or with "ctest".

include(FetchContent)

# GoogleTest is downloaded when CMake configures. The version is pinned so builds are repeatable.
FetchContent_Declare(googletest
  URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz)

# MSVC only: build GoogleTest with the same C runtime as our code, otherwise the linker complains.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
# We do not need to install GoogleTest together with MCRE.
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# Every new test file must be added to this list, just like the .cpp files in src/.
add_executable(mcre_tests
  propertyRegistryTests.cpp
  componentRegistryTests.cpp
  loaderTests.cpp
  builtinTests.cpp
)

# mcre = our engine. gtest_main supplies main() for us, so the test files have none.
target_link_libraries(mcre_tests PRIVATE mcre GTest::gtest_main)

# Registers every TEST(...) with CTest and Test Explorer, each under its own name.
include(GoogleTest)
gtest_discover_tests(mcre_tests)
