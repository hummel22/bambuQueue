.PHONY: build configure run dmg test clean

BUILD_DIR ?= build
CONFIG ?= Release

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(CONFIG)

build: configure
	cmake --build $(BUILD_DIR) --config $(CONFIG)

run: build
	./scripts/run_macos.sh

dmg: build
	./scripts/create_dmg.sh

test:
	./scripts/test.sh

clean:
	rm -rf $(BUILD_DIR) dist
