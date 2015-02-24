@echo off

call "C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\vcvarsall.bat" x64

pushd w:\c++former
IF NOT EXIST build mkdir build
pushd build
cl -Zi -Od /MD SDL2.lib SDL2main.lib SDL2test.lib SDL2_image.lib /I include ../code/hackformer.cpp /link /SUBSYSTEM:CONSOLE
popd
popd
