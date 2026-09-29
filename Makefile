.PHONY: test firmware clean

test:
	cmake -S . -B build/host -DWS_TARGET=host
	cmake --build build/host
	ctest --test-dir build/host --output-on-failure

firmware:
	cmake -S . -B build/avr -DWS_TARGET=avr -DCMAKE_TOOLCHAIN_FILE=cmake/avr-gcc.cmake
	cmake --build build/avr

clean:
	rm -rf build
