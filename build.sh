#!/bin/bash

exe=zcc
texe=ztest
cc=gcc
ldir=-Llib

src=(
    src/*.c
)

inc=(
    -I.
    -Isrc
    -Izlibc/src/include
)

libs=(
    -lutopia
    -lzlibc
)

flags=(
    -std=c89
    -nostdlib
    -nostartfiles
    -fno-stack-protector
    -Wall 
    -Wextra
    -O2
)

macos=(
    -e
    _start
    -lSystem
)

linux=(
    -e
    start
    -lgcc
    -lc
)

buildlib() {
    pushd $1 && ./build.sh $2 && mv *.a ../lib/ && popd
}

build() {
    [ ! -d lib ] && mkdir lib
    buildlib zlibc static
    buildlib utopia static
}

cmd() {
   echo "$@" && $@
}

comp() {
    [ ! -d lib ] && echo "Use 'build' before 'comp'." && exit
    if echo "$OSTYPE" | grep -q "darwin"; then
        osflags=${macos[*]}
    elif echo "$OSTYPE" | grep -q "linux"; then
        osflags=${linux[*]}
    else
        echo "This OS is not supported by this builld script yet..."
    fi
    cmd $cc ${flags[*]} ${inc[*]} $ldir ${osflags} ${libs[*]} ${src[*]} $1 -o $exe
}
    
clean() {
    [ -d lib ] && rm -r lib && echo "Deleted 'lib'."
    [ -d $exe.dSYM ] && rm -r $exe.dSYM && echo "Deleted '$exe.dSYM'."
    [ -d $texe.dSYM ] && rm -r $texe.dSYM && echo "Deleted '$texe.dSYM'."
    [ -f $exe ] && rm $exe && echo "Deleted '$exe'."
    [ -f $texe ] && rm $texe && echo "Deleted '$texe'."
    return 0
}

case "$1" in
    "build")
        build;;
    "comp")
        comp "main.c";;
    "test")
        comp "test.c";;
    "all")
        build && comp;;
    "clean")
        clean;;
    *)
        echo "Use with 'build', 'comp' or 'clean'.";;
esac
