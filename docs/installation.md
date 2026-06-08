# Building php-clickhouse

## Requirements

- PHP 8.x built with **ZTS** and the **TrueAsync** ABI (this client targets the
  TrueAsync runtime).
- A **C++17** compiler (clickhouse-cpp requires it).
- **CMake** ≥ 3.12 and a `make`/`ninja` generator.
- Standard PHP extension build tools (`phpize`, `php-config`).

## 1. Clone with submodules

```sh
git clone --recurse-submodules https://github.com/true-async/php-clickhouse.git
cd php-clickhouse
# already cloned without submodules?
git submodule update --init --recursive
```

## 2. Build the bundled clickhouse-cpp static library

The extension links a prebuilt static `clickhouse-cpp` (and its bundled contrib:
cityhash, lz4, zstd, absl). Build it once with CMake. `-DCMAKE_POSITION_INDEPENDENT_CODE=ON`
is required so the archives link into the PHP shared object.

```sh
cmake -S third_party/clickhouse-cpp -B third_party/clickhouse-cpp/build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DCH_MAP_BOOL_TO_UINT8=OFF \
  -DWITH_OPENSSL=OFF \
  -DBUILD_TESTS=OFF \
  -DBUILD_BENCHMARK=OFF
cmake --build third_party/clickhouse-cpp/build -j"$(nproc)"
```

> `-DCH_MAP_BOOL_TO_UINT8=OFF` exposes ClickHouse `Bool` as a distinct type so
> the client can return PHP `bool` (otherwise `Bool` collapses to `UInt8`/int).
>
> TLS is provided by the PHP stream layer (`tls://`), not clickhouse-cpp's own
> OpenSSL socket, so `-DWITH_OPENSSL=OFF` is fine.

## 3. Build the extension

```sh
phpize
./configure --enable-clickhouse-async
make -j"$(nproc)"
```

`configure` fails early with a clear message if step 2 was skipped.

## 4. Load it

```sh
php -d extension=$(pwd)/modules/clickhouse_async.so --ri true_async_clickhouse
```
