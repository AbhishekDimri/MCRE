# Top-level CMake file. It does global setup and then hands off to the
# sub-folders that define the actual build targets.

# Oldest CMake version this project supports. cxx_std_20 (used in src/) needs
# at least 3.12, and 3.16 is a safe modern floor.
cmake_minimum_required(VERSION 3.16)

# Hot Reload / Edit and Continue for MSVC (came with the Visual Studio template).
# Lets you edit code while debugging. It only does anything with the Microsoft
# compiler and is ignored elsewhere. Safe to leave as is.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Names the project and enables the C++ compiler. This also defines
# PROJECT_SOURCE_DIR, which the sub-files use to locate the include/ folder.
project(MCRE LANGUAGES CXX)

# Run src/CMakeLists.txt, which defines the engine library "mcre".
# This must come BEFORE app, because the app links to that library.
add_subdirectory(src)

# Run app/CMakeLists.txt, which defines the executable "mcre_app".
add_subdirectory(app)