#! /usr/bin/env bash

PLATFORM=$1
if [ "$PLATFORM" == "" ]; then
    PLATFORM=`uname`
fi
PLATFORM=`echo $PLATFORM | tr '[:upper:]' '[:lower:]'`


set -e

OPT="-O3 -g"
CXX="clang++"
CCFLAGS="$OPT -DIMGUI_DISABLE_INCLUDE_IMCONFIG_H -Iexternal -I../src"
CXXFLAGS="-c"
LDFLAGS=""
BUILDDIR="./build"
TARGET="viewer"

mkdir -p $BUILDDIR

if [ "$PLATFORM" == "darwin" ]; then
    #OPT="-O1 -g -fsanitize-address-use-after-scope -fsanitize=address -fno-omit-frame-pointer"
    ARGS="-framework Foundation $ARGS"
    APPLEFLAGS="-fobjc-arc -fmodules -x objective-c++"
    FRAMEWORKS="-framework Foundation -framework AppKit -framework Metal -framework MetalKit -framework QuartzCore"
elif [ "$PLATFORM" == "html5" ]; then
    CXX="em++"
    CXXFLAGS="-std=c++98 -Wno-c++11-compat"
    #OPT="-O0 -g"
    #LDFLAGS="--memory-init-file 0 -s WASM=1 --shell-file index.html"
    LDFLAGS="--memory-init-file 0 --shell-file index.html"
    TARGET="index.html"
fi

LIBRARYHELP=$BUILDDIR/libhelp_$PLATFORM.a
# Only comment out when the file is missing
if [ ! -e "$LIBRARYHELP" ]; then
    # ImGui and Sokol takes ~10s to compile which hinders fast iterations
    $CXX $OPT $CXXFLAGS $CCFLAGS external/imgui_draw.cpp -o $BUILDDIR/imgui_draw.o
    $CXX $OPT $CXXFLAGS $CCFLAGS external/imgui.cpp -o $BUILDDIR/imgui.o
    $CXX $OPT $CXXFLAGS $CCFLAGS $APPLEFLAGS sokol_imgui.cpp -o $BUILDDIR/sokol_imgui.o
    ar rcs $LIBRARYHELP $BUILDDIR/sokol_imgui.o $BUILDDIR/imgui.o $BUILDDIR/imgui_draw.o
else
    echo "$LIBRARYHELP already Exists!"
fi

$CXX $OPT $CXXFLAGS $CCFLAGS mapmaker.cpp -o $BUILDDIR/mapmaker.o
$CXX $OPT $CXXFLAGS $CCFLAGS viewer.cpp -o $BUILDDIR/viewer.o

$CXX $OPT $LDFLAGS $FRAMEWORKS -o $BUILDDIR/$TARGET -L$BUILDDIR -lhelp_$PLATFORM $BUILDDIR/viewer.o $BUILDDIR/mapmaker.o
