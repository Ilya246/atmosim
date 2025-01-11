mkdir -p deploy
x86_64-w64-mingw32-g++ *.cpp -o deploy/atmosim.exe -Ofast -flto=auto --static --std=c++20
echo "built atmosim.exe"
g++ *.cpp -o deploy/atmosim -Ofast -flto=auto --std=c++20
echo "built atmosim"
cd deploy
strip atmosim.exe
strip atmosim
zip atmosim_windows.zip atmosim.exe
echo "packed atmosim_windows.zip"
tar -czvf atmosim_linux.tar.gz atmosim
echo "packed atmosim_linux.tar.gz"
cd ..
