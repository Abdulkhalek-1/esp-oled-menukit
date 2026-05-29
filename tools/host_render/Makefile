BUILD_DIR := build
OUT_DIR := ../../docs/img

.PHONY: all configure build render convert clean

all: convert

configure:
	cmake -B $(BUILD_DIR) -S .

build: configure
	cmake --build $(BUILD_DIR)

render: build
	mkdir -p $(OUT_DIR)
	./$(BUILD_DIR)/render $(OUT_DIR)

convert: render
	python3 pgm_to_png.py

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(OUT_DIR)/*.pgm
