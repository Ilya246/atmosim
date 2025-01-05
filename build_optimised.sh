echo "Building profiling executable"
./build_linux.sh -fprofile-generate $@
echo "Profiling"
./fastmosim --gas1 plasma --gas2 tritium --gas3 oxygen --mixt1 375.15 --mixt2 593.15 --thirt1 293.15 --thirt2 293.15 --ticks 90 -s --simpleout --loglevel 0
echo "Building profiled executable"
./build_linux.sh -fprofile-use $@
echo "Done"
rm fastmosim-atmosim.gcda
