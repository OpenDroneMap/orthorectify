@echo off
if not exist build mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --config Debug --target ALL_BUILD -- /maxcpucount
cd ..
