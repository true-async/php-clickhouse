@echo off

rem Build the bundled clickhouse-cpp as a static library together with its
rem contrib (cityhash, lz4, zstd, absl). Runs inside the phpsdk environment, so
rem the MSVC toolset (cl/nmake) is already on PATH. A single-config "NMake
rem Makefiles" generator drops each archive in its target's build directory,
rem which is exactly where config.w32 looks for them. Flags mirror the Linux CI
rem (.github/workflows/ci.yml): native Bool, no OpenSSL (TLS goes through PHP
rem streams), no tests/benchmarks.

set CH_CPP_DIR=ext\clickhouse_async\third_party\clickhouse-cpp

if not exist "%CH_CPP_DIR%\clickhouse\client.h" (
	echo ERROR: clickhouse-cpp submodule not found at %CH_CPP_DIR%
	echo Run: git submodule update --init --recursive
	exit /b 1
)

rem Reuse a previous build (the build directory is part of the runner cache).
if exist "%CH_CPP_DIR%\build\clickhouse\clickhouse-cpp-lib.lib" (
	echo clickhouse-cpp already built, skipping.
	exit /b 0
)

rem clickhouse-cpp's bundled zstd unconditionally compiles a GAS assembly file
rem (decompress/huf_decompress_amd64.S) that MSVC cannot assemble, which breaks
rem the zstdstatic link. zstd falls back to its portable C path on MSVC
rem (ZSTD_ASM_SUPPORTED=0), so the asm source is not needed -- drop it. Patching
rem the freshly checked-out submodule here keeps our repo's submodule pointer
rem clean. Idempotent: the regex no longer matches once commented out.
powershell -NoProfile -Command "$f = '%CH_CPP_DIR%\contrib\zstd\zstd\CMakeLists.txt'; (Get-Content -LiteralPath $f) -replace '^\s*decompress/huf_decompress_amd64\.S\s*$', '        # huf_decompress_amd64.S excluded on MSVC (C fallback used)' | Set-Content -LiteralPath $f"
if %errorlevel% neq 0 exit /b 3

cmake -S "%CH_CPP_DIR%" -B "%CH_CPP_DIR%\build" -G "NMake Makefiles" ^
	-DCMAKE_BUILD_TYPE=Release ^
	-DCMAKE_C_COMPILER=cl ^
	-DCMAKE_CXX_COMPILER=cl ^
	-DBUILD_SHARED_LIBS=OFF ^
	-DCH_MAP_BOOL_TO_UINT8=OFF ^
	-DWITH_OPENSSL=OFF ^
	-DBUILD_TESTS=OFF ^
	-DBUILD_BENCHMARK=OFF
if %errorlevel% neq 0 exit /b 3

cmake --build "%CH_CPP_DIR%\build"
if %errorlevel% neq 0 exit /b 3

exit /b 0
