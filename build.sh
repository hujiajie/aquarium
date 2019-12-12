#!/bin/sh -ex

git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH=$PATH:`realpath depot_tools`
gclient

git clone https://github.com/emscripten-core/emsdk.git
./emsdk/emsdk install latest
./emsdk/emsdk activate latest
if [ -z "$BASH_SOURCE" ]
then
	BASH_SOURCE=`realpath emsdk/emsdk_env.sh`
fi
. ./emsdk/emsdk_env.sh
embuilder.py build ALL

git clone -b emscripten_angle https://github.com/webatintel/aquarium.git
cd aquarium
FQDN=`python3 -c 'from socket import *; [ai] = getaddrinfo(gethostname(), None, AF_INET, SOCK_STREAM, IPPROTO_TCP, AI_CANONNAME); print(ai[3])'`
git config user.name "$USER"
git config user.email "$USER@$FQDN"
git reset --hard emscripten_angle~1
git rebase --onto 1ed3ec6 emscripten_angle~1
gclient sync
cd -

mkdir -p aquarium/out/release
cd aquarium/out/release
tee -a ../../CMakeLists.txt <<EOF
SET_PROPERTY(TARGET aquarium APPEND_STRING PROPERTY LINK_FLAGS " -s MAIN_MODULE=0 -s FULL_ES3=0 -s DISABLE_EXCEPTION_CATCHING=1 ")
EOF
emcmake cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS_RELEASE="-DNDEBUG -O3 -g1" -DCMAKE_CXX_FLAGS_RELEASE="-DNDEBUG -O3 -g1" -DCMAKE_EXE_LINKER_FLAGS_RELEASE="-O3 -g1" ../..
ninja
cd -

install -D -m 644 aquarium/out/release/aquarium.html $INSTDIR/index.html
install -D -m 644 aquarium/out/release/aquarium.js $INSTDIR/aquarium.js
install -D -m 644 aquarium/out/release/aquarium.wasm $INSTDIR/aquarium.wasm
install -D -m 644 aquarium/out/release/aquarium.data $INSTDIR/aquarium.data
