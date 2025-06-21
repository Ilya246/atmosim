# Compiler settings
CXX := g++
CXXFLAGS := -std=c++20 -Ofast -flto=auto -Wall -Wextra -pedantic -Iinclude
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
EXEC := atmosim
WIN_EXEC := atmosim.exe
TEST_EXEC := gas_tests

.PHONY: all build win_build deploy test clean profile

all: build test

build: $(EXEC)

win_build: $(WIN_EXEC)

$(EXEC): $(SRCS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@
	@echo "Built Linux executable"

$(WIN_EXEC): $(SRCS)
	$(CROSS_PREFIX)$(CXX) $(CXXFLAGS) $(LDFLAGS) --static $^ -o $@
	@echo "Built Windows executable"

test: $(TEST_EXEC)
	./$(TEST_EXEC) --success

$(TEST_EXEC): $(TEST_SRCS) $(SRCS)
	$(CXX) $(CXXFLAGS) $^ -o $@ -lCatch2Main -lCatch2
	@echo "Built test suite"

deploy: win_build build
	mkdir -p deploy
	strip -s $(WIN_EXEC) -o deploy/$(WIN_EXEC)
	strip -s $(EXEC) -o deploy/$(EXEC)
	cd deploy && zip -q atmosim_windows.zip $(WIN_EXEC)
	cd deploy && tar -czf atmosim_linux.tar.gz $(EXEC)
	@echo "Created deployment packages"

profile: CXXFLAGS += -g
profile: build
	valgrind --dump-instr=yes --collect-jumps=yes --tool=callgrind \
	./$(EXEC) --simpleout --ticks 120 -mg=[plasma,tritium] -pg=[oxygen] \
	-m1=375.15 -m2=595.15 -t1=293.15 -t2=293.15 --silent
	kcachegrind callgrind.out.* &

clean:
	rm -f $(EXEC) $(WIN_EXEC) $(TEST_EXEC) deploy/*.zip deploy/*.tar.gz
