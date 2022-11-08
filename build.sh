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
    -lSystem
)

flags=(
    -std=c89
    -nostdlib
    -nostartfiles
    -fno-stack-protector
    -e
    _start
    -Wall 
    -Wextra
    -O2
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
        cmd $cc ${flags[*]} ${inc[*]} $ldir -lSystem ${libs[*]} ${src[*]} $1 -o $exe
    elif echo "$OSTYPE" | grep -q "linux"; then
        cmd $cc ${flags[*]} ${inc[*]} $ldir ${libs[*]} ${src[*]} $1 -o $exe
    else
        echo "This OS is not supported by this builld script yet..."
    fi
}
    
clean() {
    [ -d lib ] && rm -r lib && echo "Deleted 'lib'."
    [ -d $exe.dSYM ] && rm -r $exe.dSYM && echo "Deleted '$exe.dSYM'."
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
