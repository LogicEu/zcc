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
)

libs=(
    -lutopia 
)

flags=(
    -std=c89
    -Wall 
    -Wextra
    -O2
)

buildlib() {
    pushd $1 && ./build.sh $2 && mv *.a ../lib/ && popd
}

build() {
    [ ! -d lib ] && mkdir lib
    buildlib utopia static
}

comp() {
    [ ! -d lib ] && echo "Use 'build' before 'comp'." && exit
    echo "$cc ${flags[*]} ${inc[*]} $ldir ${libs[*]} ${src[*]} $1 -o $exe"
    $cc ${flags[*]} ${inc[*]} $ldir ${libs[*]} ${src[*]} $1 -o $exe
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
