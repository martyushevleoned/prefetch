build_dir = build
build_type = Debug

.PHONY: all clean

all:
	cmake -B $(build_dir) -DCMAKE_BUILD_TYPE=$(build_type)
	cmake --build $(build_dir) --verbose
	./$(build_dir)/src/prefetch -d ./Prefetch/ -o file.csv

clean:
	rm -rf $(build_dir)
