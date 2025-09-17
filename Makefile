CONAN ?= conan
CMAKE ?= cmake
BUILD_DIR ?= build
PRESET ?= Release

.PHONY: all setup build clean run-server run-client configure

all: build

setup:
	$(CONAN) profile detect --force || true
	$(CONAN) install . -of=$(BUILD_DIR) --build=missing -s build_type=$(PRESET)

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(PRESET) -DCMAKE_TOOLCHAIN_FILE=$(BUILD_DIR)/conan_toolchain.cmake

build: setup configure
	$(CMAKE) --build $(BUILD_DIR) --config $(PRESET)

clean:
	rm -rf $(BUILD_DIR)

run-server:
	$(BUILD_DIR)/bin/r-type_server 4242

run-client:
	$(BUILD_DIR)/bin/r-type_client
