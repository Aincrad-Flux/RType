CONAN ?= conan
CMAKE ?= cmake
BUILD_DIR ?= build
PRESET ?= Release
CONAN ?= conan


.PHONY: all setup setup-sudo build clean fclean re run-server run-client configure help

all: build

check-conan:
	@if ! command -v $(CONAN) >/dev/null 2>&1; then \
		echo "[ERROR] Conan not found. Please install it using the instructions in the README.md file."; \
		echo "        Then run: conan profile detect"; \
		exit 1; \
	fi

setup: check-conan
	# Profile detection removed to preserve custom conf values in default profile
	@if [ ! -f "$(HOME)/.conan2/profiles/default" ]; then $(CONAN) profile detect || true; fi
	@echo "[INFO] Installing dependencies with automatic system package installation..."
	@echo "[INFO] If sudo is required, you may need to run: sudo make setup"
	$(CONAN) install . -of=$(BUILD_DIR) --build=missing -s build_type=$(PRESET) -c tools.system.package_manager:mode=install || \
		(echo "[ERROR] Conan failed to install system dependencies."; \
		 echo "[INFO] This usually means system packages need to be installed with sudo."; \
		 echo "[INFO] Please run: sudo make setup"; \
		 exit 1)

setup-sudo:
	@echo "[INFO] Installing dependencies with sudo privileges..."
	$(CONAN) install . -of=$(BUILD_DIR) --build=missing -s build_type=$(PRESET) -c tools.system.package_manager:mode=install

help:
	@echo "Available targets:"; \
	echo "  build       : Install deps (Conan) + configure + build"; \
	echo "  setup       : Install dependencies (tries auto-install, suggests sudo if needed)"; \
	echo "  setup-sudo  : Install dependencies with sudo privileges"; \
	echo "  run-server  : Run server binary"; \
	echo "  run-client  : Run client binary"; \
	echo "  clean       : Remove build directory (incremental artifacts)"; \
	echo "  fclean      : Full clean (build + lib directory + root binaries)"; \
	echo "  re          : Rebuild from scratch (fclean + build)"; \
	echo "Variables:"; \
	echo "  PRESET=Release|Debug (default Release)"; \
	echo "Ensure Conan 2 installed: pip install --user conan"; \
	echo ""; \
	echo "If setup fails with permission errors, run: sudo make setup-sudo";

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
