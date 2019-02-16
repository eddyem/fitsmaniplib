cmake_minimum_required(VERSION 3.9)
project(examples)
include_directories(../)
link_libraries(usefull_macros FITSmanip cfitsio)

#add_executable(fitsstat fitsstat.c)
add_executable(keylist keylist.c)
