# for initial configuration:
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
# for build in debug mode, use the following command:
cmake --build build --config Debug
# for clean build, use the following command:
cmake --build build --config Debug --clean-first