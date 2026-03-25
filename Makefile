build_dir = build
build_type = Debug

.PHONY: all clean run

all:
	cmake -B $(build_dir) -DCMAKE_BUILD_TYPE=$(build_type) \
		-DCMAKE_CXX_COMPILER=/usr/bin/g++ \
		-DCMAKE_C_COMPILER=/usr/bin/gcc
	cmake --build $(build_dir) --verbose

run:
	LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu ./$(build_dir)/src/prefetch -d ${HOME}/Downloads/Prefetch/

clean:
	rm -rf $(build_dir)
