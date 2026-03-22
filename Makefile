build_dir = build
build_type = Release
pf_dir_path = ${HOME}/Downloads/Prefetch
output_db_file = ${build_dir}/file.db

.PHONY: all clean compare run

all:
	cmake -B $(build_dir) -DCMAKE_BUILD_TYPE=$(build_type) \
		-DCMAKE_CXX_COMPILER=/usr/bin/g++ \
		-DCMAKE_C_COMPILER=/usr/bin/gcc \
		-DLIBSCCA_PARSER=ON \
		-DSQLITE3_CONSUMER=ON
	cmake --build $(build_dir) -j $(nproc) --verbose

run: 
	LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu ./$(build_dir)/src/prefetch -d ${pf_dir_path} -o ${output_db_file}
	sqlitebrowser ${output_db_file} # apt install -y sqlitebrowser

compare:
	sccainfo ${pf_dir_path}/*\.pf # apt install -y libscca-utils

clean:
	rm -rf $(build_dir)
