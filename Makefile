ifeq ($(OS),Windows_NT)
	# FUCK MSVC FUCK MSVC FUCK MSVC FUCK MSVC FUCK MSVC
    CMAKE := cmake -G "MinGW Makefiles" -DCMAKE_CXX_COMPILER=g++
else
    CMAKE := cmake
endif

.PHONY: debug test release win web deploy

debug:
	$(CMAKE) -B out/debug -DCMAKE_BUILD_TYPE=Debug .
	@cmake --build out/debug --parallel

test:
	$(CMAKE) -B out/debug -DCMAKE_BUILD_TYPE=Test .
	@cmake --build out/debug --parallel
	@out/debug/tests

release:
	$(CMAKE) -B out/release -DCMAKE_BUILD_TYPE=Release .
	@cmake --build out/release --parallel

win:
	cmake -B out/win -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/x86_64-w64-mingw32.cmake .
	@cmake --build out/win --parallel

web:
	@emcmake cmake -B out/web -S . -DCMAKE_BUILD_TYPE=Web
	@cmake --build out/web

deploy: release win
	@mkdir -p deploy
	@tar -czf deploy/atmosim-linux-glibc-amd64.tar.gz configs -C out/release atmosim
	@zip -r deploy/atmosim-windows-amd64.zip configs
	@zip -j deploy/atmosim-windows-amd64.zip out/win/atmosim.exe
