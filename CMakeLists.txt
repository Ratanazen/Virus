cmake_minimum_required(VERSION 3.10)
target_link_libraries(MyProject PRIVATE shell32 shlwapi)   

# set the project name
project(CppProject1)

# add the executable
add_executable(CppProject1 main.cpp)
