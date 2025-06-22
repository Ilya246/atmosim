CXX ?= g++
WIN_CXX ?= x86_64-w64-mingw32-g++-win32
STRIP := strip
WIN_STRIP := x86_64-w64-mingw32-strip

STANDARD := c++20
SHAREDFLAGS ?= -Ofast -flto=auto -Wall -Wextra -pedantic -g
WIN_LDFLAGS ?= -static -static-libgcc -static-libstdc++
WEB_SHAREDFLAGS ?= -O3 -ffast-math -flto=auto -Wall -Wextra -pedantic
WEB_LDFLAGS ?= -sEXPORTED_RUNTIME_METHODS=callMain -sMODULARIZE=1 -sENVIRONMENT=web -sNO_EXIT_RUNTIME=1 -sEXPORT_NAME='createAtmosim' -sINVOKE_RUN=0
CXXFLAGS ?= $(SHAREDFLAGS) -c -std=$(STANDARD)
override CXXFLAGS += -Ilibs -Iinclude
LDFLAGS ?= $(SHAREDFLAGS)

TESTLDFLAGS ?= $(LDFLAGS) -lCatch2Main -lCatch2

LIBS := libs
SRC := src
BUILD := out
OBJ := $(BUILD)/obj
OBJ_WIN := $(BUILD)/obj_win
BINARY := atmosim
WIN_BINARY := atmosim.exe
WEB_BINARY := out/web/atmosim.js
DEPLOY := deploy

TESTSRC := tests
TESTBINARY := tests

sources := $(shell find $(SRC) -type f -name "*.cpp")
test_sources := $(shell find $(TESTSRC) -type f -name "*.cpp")

objects := $(sources:$(SRC)/%.cpp=$(OBJ)/%.o)
depends := $(sources:$(SRC)/%.cpp=$(OBJ)/%.d)

objects_win := $(sources:$(SRC)/%.cpp=$(OBJ_WIN)/%.o)
depends_win := $(sources:$(SRC)/%.cpp=$(OBJ_WIN)/%.d)

test_objects := $(filter-out $(OBJ)/main.o, $(objects)) $(test_sources:tests/%.cpp=$(OBJ)/$(TESTSRC)/%.o)
test_depends := $(test_sources:tests/%.cpp=$(OBJ)/$(TESTSRC)/%.d)

.PHONY: all build clean strip test deploy submodule web

build: $(BUILD)/$(BINARY)

all: build test

clean:
	rm -rf $(OBJ)

strip: all
	$(STRIP) $(BUILD)/$(BINARY)

test: $(BUILD)/$(TESTBINARY)
	@$(BUILD)/$(TESTBINARY)

web: $(WEB_BINARY)

deploy: $(BUILD)/$(BINARY) $(BUILD)/$(WIN_BINARY) web
	@mkdir -p $(DEPLOY)
	$(STRIP) $(BUILD)/$(BINARY) -o deploy/$(BINARY)
	$(WIN_STRIP) $(BUILD)/$(WIN_BINARY) -o deploy/$(WIN_BINARY)
	@tar -czvf $(DEPLOY)/$(BINARY)-linux-amd64.tar.gz -C $(DEPLOY) $(BINARY)
	@zip -j $(DEPLOY)/$(BINARY)-windows-amd64.zip $(DEPLOY)/$(WIN_BINARY)
	@cp web/* $(DEPLOY)/
	@echo "Created deployment archives in ./deploy/"

submodule:
	@mkdir -p $(LIBS)
	@printf "git submodule update --init --recursive"
	@git submodule update --init --recursive

$(OBJ)/%.o: $(SRC)/%.cpp
	@printf "CC\t%s\n" $@
	@mkdir -p $(@D)
	@$(CXX) $(CXXFLAGS) -MMD -MP $< -o $@

$(OBJ_WIN)/%.o: $(SRC)/%.cpp
	@printf "WIN_CC\t%s\n" $@
	@mkdir -p $(@D)
	@$(WIN_CXX) $(CXXFLAGS) $(WIN_LDFLAGS) -MMD -MP $< -o $@

$(OBJ)/$(TESTSRC)/%.o: $(TESTSRC)/%.cpp
	@printf "CC\t%s\n" $@
	@mkdir -p $(@D)
	@$(CXX) $(CXXFLAGS) -MMD -MP $< -o $@

-include $(depends) $(test_depends)

$(BUILD)/$(BINARY): $(objects)
	@printf "LD\t%s\n" $@
	@mkdir -p $(BUILD)
	@$(CXX) $^ -o $@ $(LDFLAGS)

$(BUILD)/$(WIN_BINARY): $(objects_win)
	@printf "WIN_LD\t%s\n" $@
	@mkdir -p $(BUILD)
	@$(WIN_CXX) $^ -o $@ $(LDFLAGS) $(WIN_LDFLAGS)

$(WEB_BINARY): $(sources)
	@mkdir -p $(@D)
	em++ $(WEB_SHAREDFLAGS) -std=$(STANDARD) -Ilibs -Iinclude $(sources) -o $(WEB_BINARY) $(WEB_LDFLAGS)

$(BUILD)/$(TESTBINARY): $(test_objects)
	@printf "LD\t%s\n" $@
	@mkdir -p $(BUILD)
	@$(CXX) $^ -o $@ $(TESTLDFLAGS)
