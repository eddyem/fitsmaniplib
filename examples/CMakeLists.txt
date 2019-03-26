cmake_minimum_required(VERSION 3.9)
project(examples)
include_directories(../)
link_libraries(FITSmanip cfitsio m)

#add_executable(fitsstat fitsstat.c)
add_executable(keylist keylist.c)
add_executable(imstat imstat.c)
add_executable(listtable listtable.c)
