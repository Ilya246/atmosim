# Compiler settings
CXX := g++
CXXFLAGS := -std=c++20 -Ofast -flto=auto -Wall -Wextra -pedantic -Iinclude -Ilibs
LDFLAGS := -flto=auto
CROSS_PREFIX := x86_64-w64-mingw32-
SRC_DIR := src
TEST_DIR := tests
LIB_DIR := libs

# File locations
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
TEST_SRCS := $(wildcard $(TEST_DIR)/*.cpp)
LIBS := $(wildcard $(LIB_DIR)/**/*.cpp)

# Targets
EXEC := out/atmosim
WIN_EXEC := out/atmosim.exe
TEST_EXEC := out/tests/gas_tests

.PHONY: all build win_build deploy test clean profile

all: build test

build: $(EXEC)

win_build: $(WIN_EXEC)

$(EXEC): $(SRCS)
	mkdir -p out
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@
	@echo "Built executable"

$(WIN_EXEC): $(SRCS)
	mkdir -p out
	$(CROSS_PREFIX)$(CXX) $(CXXFLAGS) $(LDFLAGS) --static $^ -o $@
	@echo "Cross-built Windows executable"

test: $(TEST_EXEC)
	./$(TEST_EXEC)

# Test sources excluding main.cpp
TEST_SRC_DEPS := $(TEST_SRCS) $(filter-out src/main.cpp, $(SRCS))

$(TEST_EXEC): $(TEST_SRC_DEPS)
	mkdir -p out/tests
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ -lCatch2Main -lCatch2
	@echo "Built test suite"

deploy: win_build build
	mkdir -p deploy
	strip -s $(WIN_EXEC) -o deploy/$(notdir $(WIN_EXEC))
	strip -s $(EXEC) -o deploy/$(notdir $(EXEC))
	cd deploy && zip -q atmosim_windows.zip $(notdir $(WIN_EXEC))
	cd deploy && tar -czf atmosim_linux.tar.gz $(notdir $(EXEC))
	@echo "Created deployment packages"

profile: CXXFLAGS += -g
profile: build
	valgrind --dump-instr=yes --collect-jumps=yes --tool=callgrind \
	./$(EXEC) --simpleout --ticks 120 -mg=[plasma,tritium] -pg=[oxygen] \
	-m1=375.15 -m2=595.15 -t1=293.15 -t2=293.15 --silent
	kcachegrind callgrind.out.* &

clean:
	rm -rf out/* deploy/*
