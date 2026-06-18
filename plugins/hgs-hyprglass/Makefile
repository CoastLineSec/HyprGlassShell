BUILD_DIR ?= build
CMAKE_BUILD_TYPE ?= RelWithDebInfo
PLUGIN_SO := $(BUILD_DIR)/hgs-hyprglass.so

.PHONY: all configure build artifact clean

all: build

configure:
	cmake -S . -B "$(BUILD_DIR)" -DCMAKE_BUILD_TYPE="$(CMAKE_BUILD_TYPE)"

build: configure
	cmake --build "$(BUILD_DIR)" --target hgs-hyprglass

artifact: build
	@printf '%s\n' "$(abspath $(PLUGIN_SO))"

clean:
	cmake --build "$(BUILD_DIR)" --target clean
