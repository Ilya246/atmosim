CXX ?= g++
STRIP := strip

STANDARD := c++20
SHAREDFLAGS ?= -Ofast -flto=auto -Wall -Wextra -pedantic -g
CXXFLAGS ?= -c -std=$(STANDARD)
override CXXFLAGS += -Ilibs -Iinclude
LDFLAGS ?= $(SHAREDFLAGS)

SRC := src
OBJ := out/obj
BUILD := out
BINARY := atmosim

sources := $(shell find $(SRC) -type f -name "*.cpp")
objects := $(sources:$(SRC)/%.cpp=$(OBJ)/%.o)
depends := $(sources:$(SRC)/%.cpp=$(OBJ)/%.d)

all: $(BUILD)/$(BINARY)

$(OBJ)/%.o: $(SRC)/%.cpp
	@printf "CC\t%s\n" $@
	@mkdir -p $(@D)
	@$(CXX) $(CXXFLAGS) -MMD -MP $< -o $@

-include $(depends)

$(BUILD)/$(BINARY): $(objects)
	@printf "LD\t%s\n" $@
	@mkdir -p $(BUILD)
	@$(CXX) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(OBJ)

strip: all
	$(STRIP) $(BUILD)/$(BINARY)

run: all
	@(BUILD)/$(BINARY)

.PHONY: all
