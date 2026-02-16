# tidesh
# 	A lightweight but fully-featured shell
# 
# This is the Makefile for the tidesh project.
# Version 1.0

# Compiler and flags
CC ?= clang
CFLAGS = -Wno-error=unused-function -Wno-error=unused-variable -Wno-error=unused-parameter -Wno-error=unused-but-set-variable -Wno-error=comment
DEBUGFLAGS = -Wall -Wextra -Werror -fsanitize=address -g
RELEASEFLAGS = -O3 -DNDEBUG
TESTINGFLAGS = -DTESTING -Itests/snow/ -DSNOW_ENABLED

# Binary name
PROJECT_NAME ?= tidesh
VERSION ?= 1.0
BRIEF ?= A lightweight but fully-featured shell

# Directory structure
SRC_DIR ?= src
INCLUDES_DIR ?= include
TESTS_DIR ?= tests
OBJ_DIR ?= obj
BIN_DIR ?= bin
DOC_DIR ?= docs

# Installation directory
PREFIX ?= /usr/local
INSTALL_DIR ?= $(PREFIX)/bin

# Include the headers
CFLAGS += -I$(INCLUDES_DIR)

# Terminal styling
BOLD := $(shell tput bold 2>/dev/null || echo '')
SGR0 := $(shell tput sgr0 2>/dev/null || echo '')
SITM := $(shell tput sitm 2>/dev/null || echo '')
SMUL := $(shell tput smul 2>/dev/null || echo '')
SETAF244 := $(shell if [ $$(tput colors 2>/dev/null || echo 0) -ge 256 ] 2>/dev/null; then tput setaf 244 2>/dev/null || echo ''; else tput setaf 0 2>/dev/null || echo ''; fi)


######################################
#               CHECKS               #
######################################

# Create necessary directories
$(shell mkdir -p $(OBJ_DIR) $(TESTS_OBJ_DIR) $(TESTS_SRC_OBJ_DIR) $(BIN_DIR) $(DOC_DIR))

# Build type configuration (release by default)
BUILD_TYPE ?= release

ifeq ($(BUILD_TYPE),release)
    CFLAGS += $(RELEASEFLAGS)
else
    CFLAGS += $(DEBUGFLAGS)
endif

# Detect platform
UNAME := $(shell uname)
ifeq ($(UNAME), Darwin)
    PLATFORM ?= macos
else ifeq ($(UNAME), Linux)
    PLATFORM ?= linux
else
    PLATFORM ?= windows
endif

# Version information from git (if available)
ifdef CI_COMMIT_SHORT_SHA
    GIT_VERSION := $(CI_COMMIT_SHORT_SHA)
# GitLab CI - full SHA that needs truncation
else ifdef CI_COMMIT_SHA
    GIT_VERSION := $(shell echo ${CI_COMMIT_SHA} | cut -c1-8)
# GitHub Actions
else ifdef GITHUB_SHA
    GIT_VERSION := $(shell echo ${GITHUB_SHA} | cut -c1-8)
else
	GIT_VERSION := $(shell git describe --tags --always 2>/dev/null || echo "unknown")
endif

BUILD_DATE := $(shell date +"%Y-%m-%d")
CFLAGS += -DPROJECT_NAME='"$(PROJECT_NAME)"' -DRAW_VERSION='"$(VERSION)"' -DGIT_VERSION='"$(GIT_VERSION)"' -DVERSION='"$(VERSION)-$(GIT_VERSION)"' -DBUILD_DATE='"$(BUILD_DATE)"' -DPLATFORM='"$(PLATFORM)"' -DBRIEF='"$(BRIEF)"'

# Source files
SRC = $(wildcard $(SRC_DIR)/*.c $(SRC_DIR)/*/*.c)

# Object files
OBJ = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC))

# Test source files
TESTS_SRC = $(wildcard $(TESTS_DIR)/*.c $(TESTS_DIR)/*/*.c)

# Test object directories
TESTING_OBJ_DIR = $(OBJ_DIR)/testing
TESTS_OBJ_DIR = $(TESTING_OBJ_DIR)/tests
TESTS_SRC_OBJ_DIR = $(TESTING_OBJ_DIR)/src

# Test object files
TESTS_OBJ = $(patsubst $(TESTS_DIR)/%.c,$(TESTS_OBJ_DIR)/%.o,$(TESTS_SRC))

# Source objects compiled with TESTING flag for test builds
TESTS_SRC_OBJ = $(patsubst $(SRC_DIR)/%.c,$(TESTS_SRC_OBJ_DIR)/%.o,$(SRC))

# Targets
TARGET_PREFIX = $(BIN_DIR)/$(PROJECT_NAME)
ifeq ($(PLATFORM), windows)
	TARGET_EXTENSION = ".exe"
else
	TARGET_EXTENSION = ""
endif
TARGET_SUFFIX = $(VERSION)-$(GIT_VERSION)-$(PLATFORM)-$(BUILD_TYPE)$(TARGET_EXTENSION)
TARGET ?= $(TARGET_PREFIX)-$(TARGET_SUFFIX)
TESTS_TARGET ?= $(TARGET_PREFIX)-test-$(TARGET_SUFFIX)

######################################
#                HELP                #
######################################

# Silent mode toggle
ifndef VERBOSE
    SILENT = @
endif

.PHONY: all
all: compile

# Help command
.PHONY: help
help:
	@echo "$(SITM)$(PROJECT_NAME) Makefile$(SGR0)"
	@echo "$(BRIEF)"
	@echo ""
	@echo "Usage:"
	@echo "  $(BOLD)make$(SGR0) <command> [BUILD_TYPE=debug|release] [VERBOSE=1]"
	@echo ""
	@echo "Build types:"
	@echo "  debug:     Build with debug info"
	@echo "  release:   Build optimized version (default)"
	@echo ""
	@echo "Available commands:"
	@echo "  $(BOLD)all:        Compile everything$(SGR0)"
	@echo "  $(BOLD)run:        Run the shell$(SGR0)"
	@echo "  $(BOLD)install:    Install the shell$(SGR0)"
	@echo "  $(BOLD)test:       Run all tests$(SGR0)"
	@echo "  $(BOLD)routine:    Run routine checks$(SGR0) $(BOLD)$(SETAF244)(clean, format, docs, lint)$(SGR0)"
	@echo ""
	@echo "Other commands:"
	@echo "  $(BOLD)info:		Show build configuration$(SGR0)"
	@echo "  $(BOLD)docs:		Generate documentation$(SGR0)"
	@echo "  $(BOLD)clean:	Clean build files$(SGR0)"
	@echo "  $(BOLD)clean/all:	Clean everything$(SGR0)"
	@echo "  $(BOLD)uninstall:	Uninstall the shell$(SGR0)"
	@echo "  $(BOLD)lint:		Run linters$(SGR0)"
	@echo "  $(BOLD)lint/tests:	Lint test code$(SGR0)"
	@echo "  $(BOLD)format:	Format code$(SGR0)"
	@echo "  $(BOLD)format/tests: Format test code$(SGR0)"
	@echo ""
	@echo "Current configuration:"
	@echo "  $(BOLD)Platform:    $(PLATFORM)$(SGR0)"
	@echo "  $(BOLD)Build type:  $(BUILD_TYPE)$(SGR0)"
	@echo "  $(BOLD)Version:     $(VERSION)-$(GIT_VERSION)$(SGR0)"
	@echo "  $(BOLD)Build date:  $(BUILD_DATE)$(SGR0)"
	@echo ""
	@echo "For more information, visit the repository at"
	@echo "  $(SMUL)https://github.com/Animenosekai/tidesh$(SGR0)"

# Info command
.PHONY: info
info:
	@echo "$(BOLD)Build Configuration:$(SGR0)"
	@echo "  $(BOLD)Platform:    $(PLATFORM)$(SGR0)"
	@echo "  $(BOLD)Build type:  $(BUILD_TYPE)$(SGR0)"
	@echo "  $(BOLD)Version:     $(VERSION)-$(GIT_VERSION)$(SGR0)"
	@echo "  $(BOLD)Build date:  $(BUILD_DATE)$(SGR0)"
	@echo "  $(BOLD)Compiler:    $(CC)$(SGR0)"
	@echo "  $(BOLD)CFLAGS:      $(CFLAGS)$(SGR0)"
	@echo "  $(BOLD)Target:      $(TARGET)$(SGR0)"


######################################
#          ROUTINE UTILITIES         #
######################################

# Routine command
.PHONY: routine
routine: clean format docs lint

# Clean commands
.PHONY: clean clean/all
clean:
	@echo "$(BOLD)🧹 Cleaning build files...$(SGR0)"
	$(SILENT)rm -rf $(OBJ_DIR)

clean/all: clean
	@echo "$(BOLD)🧹 Performing deep clean...$(SGR0)"
	$(SILENT)rm -rf $(BIN_DIR)
	$(SILENT)rm -rf $(DOC_DIR)/out

# Linting commands
.PHONY: lint lint/tests
lint:
	@echo "$(BOLD)🔍 Linting the code...$(SGR0)"
	$(SILENT)clang-tidy $(SRC)

lint/tests:
	@echo "$(BOLD)🧪 Linting the tests...$(SGR0)"
	$(SILENT)clang-tidy $(TESTS_SRC)

# Formatting commands
.PHONY: format format/tests
format:
	@echo "$(BOLD)✨ Formatting the code...$(SGR0)"
	$(SILENT)clang-format -i $(SRC) $(INCLUDES_DIR)/*.h || true

format/tests:
	@echo "$(BOLD)🧪 Formatting the tests...$(SGR0)"
	$(SILENT)clang-format -i $(TESTS_SRC) $(TESTS_DIR)/*.h || true

# Documentation command
DOXY_PROJECT_NAME = $(PROJECT_NAME)
DOXY_PROJECT_NUMBER = $(VERSION)-$(GIT_VERSION)
DOXY_PROJECT_BRIEF = $(BRIEF)
export DOXY_PROJECT_NAME
export DOXY_PROJECT_NUMBER
export DOXY_PROJECT_BRIEF
.PHONY: docs
docs:
	@echo "$(BOLD)📚 Generating the documentation...$(SGR0)"
	$(SILENT) doxygen Doxyfile || true


######################################
#             COMPILATION            #
######################################

# Source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(SILENT)mkdir -p $(dir $@)
	$(SILENT)echo "$(BOLD)🔨 Compiling $<...$(SGR0)"; \
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@; \

# Include generated dependencies
-include $(OBJ_DIR)/*.d $(TESTS_OBJ_DIR)/*.d

# Component build targets
.PHONY: compile
compile: $(OBJ)
	@echo "$(BOLD)⚙️ Core module ready$(SGR0)"

# Binary targets
$(TARGET): $(OBJ)
	@echo "$(BOLD)🔗 Linking...$(SGR0)"
	$(SILENT)$(CC) $(CFLAGS) -o $@ $^

######################################
#              RUNNING               #
######################################

# Run targets
.PHONY: run build

build: $(TARGET)
	@echo "$(BOLD)✅ Build completed: $(TARGET)$(SGR0)"

run: build
	@echo "$(BOLD)💻 Running $(PROJECT_NAME)...$(SGR0)"
	@echo "—————————————————————————————————————————————————————"
	$(SILENT)$(TARGET)
	@echo "—————————————————————————————————————————————————————"

######################################
#            INSTALLATION            #
######################################

# Install targets
.PHONY: install uninstall

install: clean/all build
	@echo "$(BOLD)📥 Installing $(PROJECT_NAME) to $(INSTALL_DIR)...$(SGR0)"
	$(SILENT)mkdir -p $(INSTALL_DIR)
	$(SILENT)cp $(TARGET) $(INSTALL_DIR)/$(PROJECT_NAME)
	$(SILENT)chmod +x $(INSTALL_DIR)/$(PROJECT_NAME)
	@echo "$(BOLD)✅ Installation completed: $(INSTALL_DIR)/$(PROJECT_NAME)$(SGR0)"

uninstall:
	@echo "$(BOLD)🗑️ Uninstalling $(PROJECT_NAME) from $(INSTALL_DIR)...$(SGR0)"
	$(SILENT)rm -f $(INSTALL_DIR)/$(PROJECT_NAME)
	@echo "$(BOLD)✅ Uninstallation completed$(SGR0)"

######################################
#               TESTING              #
######################################

# Source files with `TESTING` enabled
$(TESTS_SRC_OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "$(BOLD)🧪 Compiling $< with TESTING flags...$(SGR0)"
	$(SILENT)mkdir -p $(dir $@)
	$(SILENT)$(CC) $(CFLAGS) $(TESTINGFLAGS) -MMD -MP -c $< -o $@

# Test files
$(TESTS_OBJ_DIR)/%.o: $(TESTS_DIR)/%.c
	@echo "$(BOLD)🧪 Compiling test $<...$(SGR0)"
	$(SILENT)mkdir -p $(dir $@)
	$(SILENT)$(CC) $(CFLAGS) $(TESTINGFLAGS) -MMD -MP -c $< -o $@

# Test targets
.PHONY: test tests test/data test/core test/parsing test/execution test/integration test/builtins test/lexer test/ast test/execute test/utf8
tests: test

test: $(TESTS_TARGET)
	@echo "$(BOLD)🧪 Testing $(PROJECT_NAME)...$(SGR0)"
	@echo "—————————————————————————————————————————————————————"
	$(SILENT)$(TESTS_TARGET)
	@echo "—————————————————————————————————————————————————————"
	@echo "$(BOLD)✅ Tests completed$(SGR0)"

test/data: $(TESTS_TARGET)
	@echo "$(BOLD)🧪 Testing data structures...$(SGR0)"
	$(SILENT)$(TESTS_TARGET) array dynamic trie utf8

test/core: $(TESTS_TARGET)
	@echo "$(BOLD)🧪 Testing core components...$(SGR0)"
	$(SILENT)$(TESTS_TARGET) environ session dirstack history

test/parsing: $(TESTS_TARGET)
	@echo "$(BOLD)🧪 Testing parsing...$(SGR0)"
	$(SILENT)$(TESTS_TARGET) lexer ast

test/execution: $(TESTS_TARGET)
	@echo "$(BOLD)🧪 Testing execution...$(SGR0)"
	$(SILENT)$(TESTS_TARGET) execute

test/integration: $(TESTS_TARGET)
	@echo "$(BOLD)🧪 Testing integration...$(SGR0)"
	$(SILENT)$(TESTS_TARGET) integration

test/builtins: $(TESTS_TARGET)
	@echo "$(BOLD)🧪 Testing builtins...$(SGR0)"
	$(SILENT)$(TESTS_TARGET) builtin

test/lexer: $(TESTS_TARGET)
	$(SILENT)$(TESTS_TARGET) lexer

test/ast: $(TESTS_TARGET)
	$(SILENT)$(TESTS_TARGET) ast

test/execute: $(TESTS_TARGET)
	$(SILENT)$(TESTS_TARGET) execute

test/utf8: $(TESTS_TARGET)
	$(SILENT)$(TESTS_TARGET) utf8

# Test binary targets
$(TESTS_TARGET): $(TESTS_OBJ) $(TESTS_SRC_OBJ)
	@echo "$(BOLD)🔗 Linking tests...$(SGR0)"
	$(SILENT)$(CC) $(CFLAGS) $(TESTINGFLAGS) -o $@ $^
