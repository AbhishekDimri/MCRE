# Defines the demo executable "mcre_app". It contains only main(); all engine
# code lives in the "mcre" library.

# Builds an executable from main.cpp (path relative to this folder, app/).
# An executable may have only one main(), which is why tests will be a
# separate target later.
add_executable(mcre_app main.cpp)

# Links the executable to the engine library. This gives it the compiled
# engine code. It also inherits the include path and C++20 requirement from
# the library, because those were marked PUBLIC in src/CMakeLists.txt.
# PRIVATE: nothing that links to the app needs to know about this dependency.
target_link_libraries(mcre_app PRIVATE mcre)