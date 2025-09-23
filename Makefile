CMAKE ?= cmake
BUILD_DIR ?= build
PRESET ?= Release

.PHONY: all setup build clean fclean re run-server run-client configure help

all: build

check-conan:
	@if ! command -v $(CONAN) >/dev/null 2>&1; then \
		echo "[ERROR] Conan not found. Install it (pip install --user conan) then run: conan profile detect"; \
		exit 1; \
	fi

setup: check-conan
	@if [ ! -f "$(HOME)/.conan2/profiles/default" ]; then $(CONAN) profile detect || true; fi
	@echo "[INFO] Installing dependencies with sudo (apt)."
	$(CONAN) install . -of=$(BUILD_DIR) --build=missing -s build_type=$(PRESET) \
		-c tools.system.package_manager:mode=install \
		-c tools.system.package_manager:sudo=True

configure:
	@toolchain_file=$$(find $(BUILD_DIR) -name conan_toolchain.cmake -print -quit); \
	if [ -z "$$toolchain_file" ]; then \
		echo "[ERROR] conan_toolchain.cmake not found. Run 'make setup' first."; \
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

help:
	@echo "Targets: build setup clean fclean re run-server run-client"
	@echo "Vars: PRESET=Release|Debug (default Release)"
