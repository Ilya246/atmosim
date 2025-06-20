profile_command="./atmosim --simpleout --ticks 120 -mg=[plasma,tritium] -pg=[oxygen] -m1=375.15 -m2=595.15 -t1=293.15 -t2=293.15 --silent"

echo "Building with debug symbols"
./build_linux.sh -g

echo "Timing normal run"
time $profile_command

for file in callgrind.out.*; do
	echo "mv $file old.$file"
	mv $file "old.$file"
done

echo "Using callgrind, output redirected to profile_out.txt"
valgrind --dump-instr=yes --collect-jumps=yes --tool=callgrind $profile_command

echo "Done"
kcachegrind callgrind.out.* &
