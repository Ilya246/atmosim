ifeq ($(OS),Windows_NT)
	# FUCK MSVC FUCK MSVC FUCK MSVC FUCK MSVC FUCK MSVC
    CMAKE := cmake -G "MinGW Makefiles" -DCMAKE_CXX_COMPILER=g++
else
    CMAKE := cmake
endif

.PHONY: debug test release win web deploy

debug:
	$(CMAKE) -B build -DCMAKE_BUILD_TYPE=Debug .
	@cmake --build build --parallel

test:
	$(CMAKE) -B build -DCMAKE_BUILD_TYPE=Test .
	@cmake --build build --parallel
	@build/tests

release-tui:
	$(CMAKE) -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_GUI=OFF -DBUILD_TUI=ON .
	@cmake --build build --parallel

release:
	$(CMAKE) -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_GUI=ON -DBUILD_TUI=OFF .
	@cmake --build build --parallel

win-tui:
	cmake -B build/win -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/x86_64-w64-mingw32.cmake .
	@cmake --build build/win --parallel

# TODO: fix
#win:
#	cmake -B build/win -DCMAKE_BUILD_TYPE=Release -DBUILD_GUI=ON -DCMAKE_TOOLCHAIN_FILE=cmake/x86_64-w64-mingw32.cmake .
#	@cmake --build build/win --parallel

web:
	@emcmake cmake -B build/web -S . -DCMAKE_BUILD_TYPE=Web
	@cmake --build build/web --parallel

deploy: release release-tui win-tui
	@mkdir -p deploy
	@tar -czf deploy/atmosim-linux-glibc-amd64.tar.gz configs -C build atmosim
	@tar -czf deploy/atmosim-gui-linux-glibc-amd64.tar.gz configs -C build atmosim_gui
	@zip -r deploy/atmosim-windows-amd64.zip configs
	@zip -j deploy/atmosim-windows-amd64.zip build/win/atmosim.exe
