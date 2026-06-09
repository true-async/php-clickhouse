@echo off
rem No EnableDelayedExpansion: it would treat '!' specially and corrupt PHP code.
setlocal

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

rem Bring dependency DLLs (libuv, OpenSSL, ...) next to php.exe so it can start.
call %~dp0find-target-branch.bat
set DEPS_DIR=%PHP_BUILD_CACHE_BASE_DIR%\deps-%BRANCH%-%PHP_SDK_VS%-%PHP_SDK_ARCH%
if exist "%DEPS_DIR%\bin" copy /y "%DEPS_DIR%\bin\*.dll" "%PHP_BUILD_DIR%\" >nul

echo.
echo --- php -m ---
"%PHP_BUILD_DIR%\php.exe" -n -d extension_dir="%PHP_BUILD_DIR%\ext" -d extension=php_clickhouse_async.dll -m
echo.

rem The extension is built as a shared DLL (config.w32 EXTENSION third arg = true).
rem Load it explicitly via -d extension= since -n skips ini files.
echo --- verifying clickhouse_async ---
"%PHP_BUILD_DIR%\php.exe" -n -d extension_dir="%PHP_BUILD_DIR%\ext" -d extension=php_clickhouse_async.dll "%~dp0smoke.php"
set RC=%errorlevel%

echo.
echo smoke test exit code: %RC%
exit /b %RC%
