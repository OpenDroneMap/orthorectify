@echo off
if not exist vcpkg\vcpkg.exe (
    cd vcpkg
    call bootstrap-vcpkg.bat -disableMetrics
    cd ..
)
cmake --preset windows-release
cmake --build --preset windows-release
