add_executable(demo demo.cpp window.cpp)
target_link_libraries(demo glad SDL2-static) # SDL2-static or SDL2 as shared library
