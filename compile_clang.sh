PWD=`pwd`
BUILDDIR=$PWD/build/darwin
CXXFLAGS="-g -O3 -m64 -stdlib=libc++ -Wall -Weverything -pedantic"
LDFLAGS=""
COMPILER=clang++
LINKER=libtool

if [ ! -d "$BUILDDIR" ]; then
  mkdir -p $BUILDDIR
fi

$COMPILER -c src/external/stblib.c -o $BUILDDIR/stblib.o -g -O3 -m64
$LINKER -static -o $BUILDDIR/libstblib.a $BUILDDIR/stblib.o

$COMPILER \
        -I. \
        -L${BUILDDIR} \
        -lstblib \
        -o $BUILDDIR/main \
        $CXXFLAGS \
        src/main.cpp
