@echo off
if not exist build mkdir build
windres resource.rc -O coff -o resource.o
g++ main.cpp resource.o -o build/cclank.exe -static -lshlwapi
if exist resource.o del resource.o
echo Build complete: build/cclank.exe