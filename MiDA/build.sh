#!/bin/bash

build_cpp () {
    name=$(dirname $1)/$(basename $1 .cpp)
    common="-std=c++17 -W -Wall -Wno-unused-parameter"
    # g++ ${common}           -D VERBOSE=true      -g $1 -o ${name}.exe
    # g++ ${common} -D NDEBUG -D VERBOSE=true  -Og -g $1 -o ${name}.exe
    g++ ${common} -D NDEBUG -D VERBOSE=true  -O3    $1 -o ${name}.exe
    # g++ ${common} -D NDEBUG -D VERBOSE=false -O3    $1 -o ${name}.exe
}

for var in "$@"; do
    echo "$var"
    build_cpp "$var"
done
