CONAN ?= conan
CMAKE ?= cmake
BUILD_DIR ?= build
PRESET ?= Release

.PHONY: all setup build clean fclean re run-server run-client configure help

all: build

check-conan:
	@if ! command -v $(CONAN) >/dev/null 2>&1; then \
		echo "[ERROR] Conan not found. Install with: pip install --user conan"; \
		echo "        Then run: conan profile detect"; \
		exit 1; \
	fi

setup: check-conan
	# Profile detection removed to preserve custom conf values in default profile
	@if [ ! -f "$(HOME)/.conan2/profiles/default" ]; then $(CONAN) profile detect || true; fi
	$(CONAN) install . -of=$(BUILD_DIR) --build=missing -s build_type=$(PRESET)

help:
	@echo "Available targets:"; \
	echo "  build       : Install deps (Conan) + configure + build"; \
	echo "  run-server  : Run server binary"; \
	echo "  run-client  : Run client binary"; \
	echo "  clean       : Remove build directory (incremental artifacts)"; \
	echo "  fclean      : Full clean (build + lib directory + root binaries)"; \
	echo "  re          : Rebuild from scratch (fclean + build)"; \
	echo "Variables:"; \
	echo "  PRESET=Release|Debug (default Release)"; \
	echo "Ensure Conan 2 installed: pip install --user conan";

configure:
	@toolchain_file=$$(find $(BUILD_DIR) -name conan_toolchain.cmake -print -quit); \
	if [ -z "$$toolchain_file" ]; then \
		echo "[ERROR] conan_toolchain.cmake not found under $(BUILD_DIR). Run 'make setup' first."; \
		exit 1; \
	fi; \
	echo "[INFO] Using toolchain: $$toolchain_file"; \
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(PRESET) -DCMAKE_TOOLCHAIN_FILE=$$toolchain_file

build: setup configure
	$(CMAKE) --build $(BUILD_DIR) --config $(PRESET)

clean:
	rm -rf $(BUILD_DIR)

fclean: clean
	rm -rf lib
	rm -f r-type_client r-type_server

re: fclean all

run-server:
	$(BUILD_DIR)/bin/r-type_server 4242

run-client:
	$(BUILD_DIR)/bin/r-type_client
