@echo off

call "C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\vcvarsall.bat"

pushd w:\c++former
IF NOT EXIST build mkdir build
pushd build

del *.pdb

cl -Zi -Od -W3 -WX -EHsc -MD -F 8000000 SDL2.lib SDL2main.lib SDL2test.lib SDL2_image.lib SDL2_mixer.lib glew32.lib glew32s.lib opengl32.lib SDL2_ttf.lib -I include ../code/hackformer_collisionDataTool.cpp -link -INCREMENTAL:NO -SUBSYSTEM:WINDOWS

popd
popd
