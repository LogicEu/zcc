#!/bin/bash

name=zcc
cc=gcc

tmpdir=tmp
srcdir=src
libdir=lib

flags=(
    -std=c89
    -Wall
    -Wextra
    -pedantic
    -O2
    -Izlibc/src/include
    -Iutopia
    -Isrc
)

nostd=(
    -nostdlib
    -nostarfiles
    -fno-stack-protector
    -e
)

libs=(
    -lzlibc
    -lutopia
)

if echo "$OSTYPE" | grep -q "darwin"; then
    oslib=(
        -lSystem
    )
    entry=_start
elif echo "$OSTYPE" | grep -q "linux"; then
    oslib=(
        -lgcc 
        -lc
    )
    entry=-start
else
    echo "This OS is not supported yet..." && exit
fi

cmd() {
    echo "$@" && $@ || exit
}

check() {
    [ -f $1.a ] || [ -f $1.so ] || [ -f $1.dylib ]
}

checkargs() {
    for arg in $@
    do
        check $libdir/lib$arg || echo "Use 'build' to compile dependencies." && exit
    done
}

comp() {
    checkargs zlibc utopia
    cmd mkdir -p $tmpdir
    cmd $cc -c $src ${flags[*]}
    cmd mv *.o $tmpdir/
    cmd $cc $tmpdir/*.o -o $name ${nostd[*]} -e $entry -L$libdir ${libs[*]} ${oslib[*]}
}

buildlib() {
    cmd ./$1/build.sh all && cmd cp bin/* ../$libdir
}

build() {
    cmd mkdir -p $libdir
    buildlib zlibc
    buildlib utopia
}

cleand() {
    [ -d $1 ] && cmd rm -r $1
}

cleanf() {
    [ -f $1 ] && cmd rm $1
}

cleandir() {
    pushd $1 && ./build.sh clean && popd
}

clean() {
    cleandir zlibc
    cleandir utopia
    cleanf a.out
    cleanf $name
    cleand $libdir
    cleand $tmpdir
    return 0
}

install() {
    [ "$EUID" -ne 0 ] && echo "Run with sudo to install" && exit
    
    build && comp
    [ -f $name ] && mv $name /usr/local/bin/
    
    echo "Successfully installed $name"
    return 0
}

uninstall() {
    [ "$EUID" -ne 0 ] && echo "Run with sudo to uninstall" && exit

    cleanf /usr/local/bin/$name

    echo "Successfully uninstalled $name"
    return 0
}

case "$1" in
    "build")
        build;;
    "comp")
        comp;;
    "all")
        build && comp;;
    "make")
        make;;
    "clean")
        clean;;
    "install")
        install;;
    "uninstall")
        uninstall;;
    *)
        echo "Run with 'build' to compile dependencies or 'comp' to build executable"
        echo "Use 'install' to build and install in /usr/local/bin"
        echo "Use 'clean' to remove local builds"
esac
