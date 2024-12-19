profile_command="./atmosim --gas1 plasma --gas2 tritium --gas3 oxygen --mixt1 375.15 --mixt2 593.15 --thirt1 293.15 --thirt2 293.15 --doretest N --ticks 90 -s"

echo "Building with debug symbols"
./build_linux.sh -g

echo "Timing normal run"
time $profile_command

echo "Using callgrind, output redirected to profile_out.txt"
valgrind --dump-instr=yes --collect-jumps=yes --tool=callgrind $profile_command > profile_out.txt

echo "Done"
kcachegrind callgrind.out.* &
