if ! [ -d "build/" ]; then
    ./rebuild.sh
else
    cd build/
    cmake --build .
fi
