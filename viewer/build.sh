#! /usr/bin/env bash

set -e
OPT="-O3 -g"
#OPT="-O1 -g -fsanitize-address-use-after-scope -fsanitize=address -fno-omit-frame-pointer"
CXX="clang++"
CCFLAGS="$OPT -DIMGUI_DISABLE_INCLUDE_IMCONFIG_H -Iexternal -I../src"
CXXFLAGS="-c"
APPLEFLAGS="-fobjc-arc -fmodules -x objective-c++"
LDFLAGS="$OPT"
FRAMEWORKS="-framework Foundation -framework AppKit -framework Metal -framework MetalKit -framework QuartzCore"
BUILDDIR="./build"

mkdir -p $BUILDDIR

LIBRARYHELP=$BUILDDIR/libhelp.a
# Only comment out when the file is missing
if [ ! -e "$LIBRARYHELP" ]; then
    $CXX $CXXFLAGS $CCFLAGS external/imgui_draw.cpp -o $BUILDDIR/imgui_draw.o
    $CXX $CXXFLAGS $CCFLAGS external/imgui.cpp -o $BUILDDIR/imgui.o
    $CXX $CXXFLAGS $CCFLAGS $APPLEFLAGS external/sokol_imgui.cpp -o $BUILDDIR/sokol_imgui.o
    ar rcs $BUILDDIR/libhelp.a $BUILDDIR/sokol_imgui.o $BUILDDIR/imgui.o $BUILDDIR/imgui_draw.o
else
    echo "$LIBRARYHELP already Exists!"
fi

$CXX $CXXFLAGS $CCFLAGS mapmaker.cpp -o $BUILDDIR/mapmaker.o
$CXX $CXXFLAGS $CCFLAGS viewer.cpp -o $BUILDDIR/viewer.o

$CXX $LDFLAGS $FRAMEWORKS -o $BUILDDIR/viewer -L$BUILDDIR -lhelp $BUILDDIR/viewer.o $BUILDDIR/mapmaker.o
