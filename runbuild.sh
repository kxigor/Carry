#!/bin/bash

build() {
    cmake . -B build
    cmake --build build --parallel 4
    # make -C build -s
}

clean_build() {
    make -C build clean >/dev/null
}

erase_build() {
    clean_build
    rm -rf build >/dev/null
}

if [ "$1" == "erase" ]; then
    erase_build
elif [ "$1" == "clean" ]; then
    clean_build
else
    build
fi