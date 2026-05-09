#!/bin/sh

CORES="$(nproc --all)"

SRC="Source/Atmosphere/stratosphere/loader/"
DEST="build/stratosphere/loader/"
mkdir -p "dist/atmosphere/kips/"
mkdir -p "$DEST"

cp -r "$SRC"/. "$DEST"/

echo "CORES: $CORES"

cd build/stratosphere/loader || exit 1
make -j$CORES
hactool -t kip1 out/nintendo_nx_arm64_armv8a/release/loader.kip --uncompress=hoc.kip
cd ../../../ # exit
cp build/stratosphere/loader/hoc.kip dist/atmosphere/kips/hoc.kip

cd Source/hoc-clk/
./build.sh
cp -r dist/ ../../

cd ../../

cd Source/Horizon-OC-Monitor/
make -j$CORES
cp Horizon-OC-Monitor.ovl ../../dist/switch/.overlays/Horizon-OC-Monitor.ovl