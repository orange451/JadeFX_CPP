# Unix: make && make run
# Windows (from a developer prompt): cmake -S . -B build && cmake --build build --config Release
#
# Which sample `make run` launches. Cmd+Shift+B runs that same target.
# purple — the gradient phone screen
# hello  — a 320×240 "Hello World" window
# border — a BorderPane with top, left, center, right, and bottom
# tabs   — a TabPane with three closable pages
# tree   — a material TreeView with disclosure arrows and a selection bar
# split  — a SplitPane with draggable dividers
# rich     — a code editor and a styled text area
# controls — text field, combo box, radios, toggles, menus, tooltips, and alerts
SAMPLE ?= tabs

BUILD_DIR := build

ifeq ($(SAMPLE),hello)
CMAKE_TARGET := jadefx-hello
ifeq ($(shell uname),Darwin)
APP_BIN := $(BUILD_DIR)/JadeFX Hello.app/Contents/MacOS/JadeFX Hello
else
APP_BIN := $(BUILD_DIR)/jadefx-hello
endif
else ifeq ($(SAMPLE),purple)
CMAKE_TARGET := jadefx-purple
ifeq ($(shell uname),Darwin)
APP_BIN := $(BUILD_DIR)/JadeFX.app/Contents/MacOS/JadeFX
else
APP_BIN := $(BUILD_DIR)/jadefx-purple
endif
else ifeq ($(SAMPLE),border)
CMAKE_TARGET := jadefx-border
ifeq ($(shell uname),Darwin)
APP_BIN := $(BUILD_DIR)/JadeFX Border.app/Contents/MacOS/JadeFX Border
else
APP_BIN := $(BUILD_DIR)/jadefx-border
endif
else ifeq ($(SAMPLE),tabs)
CMAKE_TARGET := jadefx-tabs
ifeq ($(shell uname),Darwin)
APP_BIN := $(BUILD_DIR)/JadeFX Tabs.app/Contents/MacOS/JadeFX Tabs
else
APP_BIN := $(BUILD_DIR)/jadefx-tabs
endif
else ifeq ($(SAMPLE),tree)
CMAKE_TARGET := jadefx-tree
ifeq ($(shell uname),Darwin)
APP_BIN := $(BUILD_DIR)/JadeFX Tree.app/Contents/MacOS/JadeFX Tree
else
APP_BIN := $(BUILD_DIR)/jadefx-tree
endif
else ifeq ($(SAMPLE),split)
CMAKE_TARGET := jadefx-split
ifeq ($(shell uname),Darwin)
APP_BIN := $(BUILD_DIR)/JadeFX Split.app/Contents/MacOS/JadeFX Split
else
APP_BIN := $(BUILD_DIR)/jadefx-split
endif
else ifeq ($(SAMPLE),rich)
CMAKE_TARGET := jadefx-rich
ifeq ($(shell uname),Darwin)
APP_BIN := $(BUILD_DIR)/JadeFX Rich.app/Contents/MacOS/JadeFX Rich
else
APP_BIN := $(BUILD_DIR)/jadefx-rich
endif
else ifeq ($(SAMPLE),controls)
CMAKE_TARGET := jadefx-controls
ifeq ($(shell uname),Darwin)
APP_BIN := $(BUILD_DIR)/JadeFX Controls.app/Contents/MacOS/JadeFX Controls
else
APP_BIN := $(BUILD_DIR)/jadefx-controls
endif
else
$(error Unknown sample "$(SAMPLE)". Use purple, hello, border, tabs, tree, split, rich, or controls.)
endif

.PHONY: all run test clean purple hello border tabs tree split rich controls

all:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR) --parallel

test: all
	cmake --build $(BUILD_DIR) --target jadefx-tests --parallel
	$(BUILD_DIR)/jadefx-tests

run:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR) --target $(CMAKE_TARGET) --parallel
	"$(APP_BIN)"

purple hello border tabs tree split rich controls:
	$(MAKE) run SAMPLE=$@

clean:
	rm -rf $(BUILD_DIR)
