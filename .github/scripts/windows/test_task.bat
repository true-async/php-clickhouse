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

rem Bring dependency DLLs (libuv, ...) next to php.exe so it can start.
call %~dp0find-target-branch.bat
set DEPS_DIR=%PHP_BUILD_CACHE_BASE_DIR%\deps-%BRANCH%-%PHP_SDK_VS%-%PHP_SDK_ARCH%
if exist "%DEPS_DIR%\bin" copy /y "%DEPS_DIR%\bin\*.dll" "%PHP_BUILD_DIR%\" >nul

rem In-source PHP builds place shared extension DLLs in the build root next to
rem php.exe, not in ext\. Use the full path so -n (no ini) still loads the DLL.
set CH_DLL=%PHP_BUILD_DIR%\php_clickhouse_async.dll
if not exist "%CH_DLL%" (
    echo ERROR: php_clickhouse_async.dll not found at %CH_DLL%
    exit /b 1
)

echo.
echo --- php -m ---
"%PHP_BUILD_DIR%\php.exe" -n -d extension="%CH_DLL%" -m
echo.

echo --- verifying clickhouse_async ---
"%PHP_BUILD_DIR%\php.exe" -n -d extension="%CH_DLL%" "%~dp0smoke.php"
set RC=%errorlevel%

echo.
echo smoke test exit code: %RC%
exit /b %RC%
