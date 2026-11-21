
default_build_type := 'Debug'

set default-list := true

configure BUILD_TYPE=default_build_type:
    cmake -S ether -B build -DCMAKE_BUILD_TYPE={{BUILD_TYPE}}

build: 
    cmake --build build

run: build configure
    ./build/ether