cmake_minimum_required(VERSION 3.11)
project(LinuxKernelMapTUI)

set(CMAKE_CXX_STANDARD 17)

# Download FTXUI automatically
include(FetchContent)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
  GIT_TAG        v5.0.0
)
FetchContent_MakeAvailable(ftxui)

add_executable(kmap main.cpp)
target_link_libraries(kmap PRIVATE ftxui::screen ftxui::dom ftxui::component)