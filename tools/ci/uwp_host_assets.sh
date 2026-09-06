#!/usr/bin/env bash
set -euo pipefail

git submodule update --init --depth 1 Shipwright/libultraship/extern/StormLib
git clone --depth 1 --branch zelda3d https://github.com/SomeoneIsWorking/ZAPDTR.git Shipwright/ZAPDTR
mkdir -p scratch
base64 --decode cmake/Zelda3DZAPDTR-5f37af8.bundle.b64 > scratch/ZAPDTR-5f37af8.bundle
echo "${ZAPDTR_BUNDLE_SHA256}  scratch/ZAPDTR-5f37af8.bundle" | sha256sum --check --strict
test "$(git -C Shipwright/ZAPDTR rev-parse HEAD)" = "${ZAPDTR_BASE_COMMIT}"
git -C Shipwright/ZAPDTR fetch "$GITHUB_WORKSPACE/source/scratch/ZAPDTR-5f37af8.bundle" HEAD
git -C Shipwright/ZAPDTR checkout --detach FETCH_HEAD
test "$(git -C Shipwright/ZAPDTR rev-parse HEAD)" = "${ZAPDTR_COMMIT}"

sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  ninja-build lsb-release zipcmp zipmerge ziptool \
  libsdl2-dev libsdl2-net-dev libpng-dev \
  libzip-dev nlohmann-json3-dev libtinyxml2-dev libspdlog-dev \
  libopengl-dev libopusfile-dev libvorbis-dev libfreetype6-dev

cmake -S . -B scratch/build-host-assets -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DZELDA3D_SDL2_OPENGL=ON \
  -DZELDA3D_BUILD_MM=OFF \
  -DZELDA3D_BUILD_LAUNCHER=OFF \
  -DZELDA3D_ENABLE_ROM_EXTRACTION=ON \
  -DLUS_BUILD_TESTS=OFF

cmake --build scratch/build-host-assets --target GenerateSohOtr --parallel 2
test -s Shipwright/soh/soh.o2r
unzip -t Shipwright/soh/soh.o2r
