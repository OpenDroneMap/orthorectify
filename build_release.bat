@echo off
if not exist build mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release --target ALL_BUILD -- /maxcpucount
cd ..
