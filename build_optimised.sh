echo "Building profiling executable"
g++ atmosim.cpp -o fastmosim -Ofast -std=c++20 -mtune=native -march=native -Wall -Wextra -pedantic -flto -fprofile-generate $@
echo "Profiling"
./fastmosim --gas1 plasma --gas2 tritium --gas3 oxygen --mixt1 375.15 --mixt2 593.15 --thirt1 293.15 --thirt2 293.15 --doretest N --ticks 90 -s --simpleout --loglevel 0
echo "Building profiled executable"
g++ atmosim.cpp -o fastmosim -Ofast -std=c++20 -mtune=native -march=native -Wall -Wextra -pedantic -flto -fprofile-use $@
echo "Done"
rm fastmosim-atmosim.gcda
