x86_64-w64-mingw32-g++ atmosim.cpp -o deploy/atmosim.exe -O3 --static --std=c++20
echo "built atmosim.exe"
g++ atmosim.cpp -o deploy/atmosim -O3 --static --std=c++20
echo "built atmosim"
