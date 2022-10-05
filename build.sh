#!/bin/bash

exe=eulang
cc=gcc
inc=-I.
ldir=-Llib

src=(
    src/*.c
)

libs=(
    -lutopia 
)

flags=(
    -std=c99
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
    echo "$cc ${flags[*]} $inc $ldir ${libs[*]} ${src[*]} -o $exe"
    $cc ${flags[*]} $inc $ldir ${libs[*]} ${src[*]} -o $exe
}
    
clean() {
    [ -d lib ] && rm -r lib && echo "Deleted 'lib'."
    [ -d $exe.dSYM ] && rm -r $exe.dSYM && echo "Deleted '$exe.dSYM'."
    [ -f $exe ] && rm $exe && echo "Deleted '$exe'."
    return 0
}

case "$1" in
    "build")
        build;;
    "comp")
        comp;;
    "all")
        build && comp;;
    "clean")
        clean;;
    *)
        echo "Use with 'build', 'comp' or 'clean'.";;
esac
