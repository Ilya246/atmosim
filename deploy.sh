x86_64-w64-mingw32-g++ atmosim.cpp -o deploy/atmosim.exe -O3 --static --std=c++20
echo "built atmosim.exe"
g++ atmosim.cpp -o deploy/atmosim -O3 --static --std=c++20
echo "built atmosim"
cd deploy
zip atmosim_windows.zip atmosim.exe
echo "packed atmosim_windows.zip"
tar -czvf atmosim_linux.tar.gz atmosim
echo "packed atmosim_linux.tar.gz"
cd ..
