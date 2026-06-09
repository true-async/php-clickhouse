@echo off
setlocal EnableDelayedExpansion

if /i "%GITHUB_ACTIONS%" neq "True" (
    echo for CI only
    exit /b 3
)

echo === clickhouse_async smoke test ===

set PHP_BUILD_DIR=%PHP_BUILD_OBJ_DIR%\Release_TS
if not exist "%PHP_BUILD_DIR%\php.exe" (
	echo ERROR: php.exe not found at %PHP_BUILD_DIR%
	exit /b 1
)

rem Bring dependency DLLs (libuv, ...) next to php.exe so it can start.
call %~dp0find-target-branch.bat
set DEPS_DIR=%PHP_BUILD_CACHE_BASE_DIR%\deps-%BRANCH%-%PHP_SDK_VS%-%PHP_SDK_ARCH%
if exist "%DEPS_DIR%\bin" copy /y "%DEPS_DIR%\bin\*.dll" "%PHP_BUILD_DIR%\" >nul

echo.
echo --- php -m ---
"%PHP_BUILD_DIR%\php.exe" -n -m
echo.

rem The extension is built statically into php.exe, so no ini is needed (-n).
rem It registers under the module name "true_async_clickhouse" and exposes the
rem TrueAsync\ClickHouse\Client class; both must be present for the build to be
rem considered good.
echo --- verifying clickhouse_async ---
"%PHP_BUILD_DIR%\php.exe" -n -r "if (!extension_loaded('true_async_clickhouse')) { fwrite(STDERR, 'FAIL: true_async_clickhouse not loaded' . PHP_EOL); exit(1); } if (!class_exists('TrueAsync\\ClickHouse\\Client')) { fwrite(STDERR, 'FAIL: TrueAsync\\ClickHouse\\Client missing' . PHP_EOL); exit(1); } echo 'OK: true_async_clickhouse loaded, TrueAsync\\ClickHouse\\Client present' . PHP_EOL;"
set RC=%errorlevel%

echo.
echo smoke test exit code: %RC%
exit /b %RC%
