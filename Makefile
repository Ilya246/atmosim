CXX ?= g++
STRIP := strip

STANDARD := c++20
SHAREDFLAGS ?= -Ofast -flto=auto -Wall -Wextra -pedantic -g
CXXFLAGS ?= $(SHAREDFLAGS) -c -std=$(STANDARD)
override CXXFLAGS += -Ilibs -Iinclude
LDFLAGS ?= $(SHAREDFLAGS)

TESTLDFLAGS ?= $(LDFLAGS) -lCatch2Main -lCatch2

LIBS := libs
SRC := src
OBJ := out/obj
BUILD := out
BINARY := atmosim

SUBMODULES := argparse

TESTSRC := tests
TESTBINARY := tests

sources := $(shell find $(SRC) -type f -name "*.cpp")
objects := $(sources:$(SRC)/%.cpp=$(OBJ)/%.o)
depends := $(sources:$(SRC)/%.cpp=$(OBJ)/%.d)
submodules := $(SUBMODULES:%=$(LIBS)/%)

test_sources := $(shell find $(TESTSRC) -type f -name "*.cpp")
test_objects := $(filter-out $(OBJ)/main.o, $(objects)) $(test_sources:tests/%.cpp=$(OBJ)/$(TESTSRC)/%.o)
test_depends := $(test_sources:tests/%.cpp=$(OBJ)/$(TESTSRC)/%.d)

build: $(BUILD)/$(BINARY)

all: build test

clean:
	rm -rf $(OBJ)

strip: all
	$(STRIP) $(BUILD)/$(BINARY)

test: $(BUILD)/$(TESTBINARY)
	@$(BUILD)/$(TESTBINARY)

$(OBJ)/%.o: $(SRC)/%.cpp $(submodules)
	@printf "CC\t%s\n" $@
	@mkdir -p $(@D)
	@$(CXX) $(CXXFLAGS) -MMD -MP $< -o $@

$(OBJ)/$(TESTSRC)/%.o: $(TESTSRC)/%.cpp
	@printf "CC\t%s\n" $@
	@mkdir -p $(@D)
	@$(CXX) $(CXXFLAGS) -MMD -MP $< -o $@

$(LIBS)/%:
	@git submodule init $@
	@git submodule update $@

-include $(depends) $(test_depends)

$(BUILD)/$(BINARY): $(objects)
	@printf "LD\t%s\n" $@
	@mkdir -p $(BUILD)
	@$(CXX) $^ -o $@ $(LDFLAGS)

$(BUILD)/$(TESTBINARY): $(test_objects)
	@printf "LD\t%s\n" $@
	@mkdir -p $(BUILD)
	@$(CXX) $^ -o $@ $(TESTLDFLAGS)

.PHONY: all build clean strip test
