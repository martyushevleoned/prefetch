# Set-ExecutionPolicy -ExecutionPolicy Bypass -Scope Process
rm -r build
cmake -S . -B build -G "MinGW Makefiles" -DNTDLL_PARSER=ON -DSQLITE3_CONSUMER=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 10 --verbose
.\build\src\prefetch.exe -d C:\Users\User\Downloads\Prefetch -o ./build/file.db