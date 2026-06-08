dnl config.m4 for extension clickhouse_async
dnl
dnl Native asynchronous ClickHouse client for PHP TrueAsync, built on the
dnl official clickhouse-cpp native-protocol library (bundled as a submodule
dnl under third_party/clickhouse-cpp).

PHP_ARG_ENABLE([clickhouse-async],
  [whether to enable clickhouse_async support],
  [AS_HELP_STRING([--enable-clickhouse-async],
    [Enable clickhouse_async support])],
  [no])

if test "$PHP_CLICKHOUSE_ASYNC" != "no"; then

  dnl clickhouse-cpp requires a C++17 compiler.
  PHP_REQUIRE_CXX()
  AC_LANG_PUSH([C++])
  CLICKHOUSE_ASYNC_STD="-std=c++17"
  AC_LANG_POP([C++])

  dnl Link the C++ standard library into the shared object.
  PHP_ADD_LIBRARY([stdc++], [1], [CLICKHOUSE_ASYNC_SHARED_LIBADD])

  dnl clickhouse-cpp public headers live at the submodule root, e.g.
  dnl <clickhouse/client.h>.
  CLICKHOUSE_CPP_DIR="$abs_srcdir/third_party/clickhouse-cpp"
  PHP_ADD_INCLUDE([$CLICKHOUSE_CPP_DIR])
  dnl clickhouse-cpp public headers pull in absl (Int128); its contrib root.
  PHP_ADD_INCLUDE([$CLICKHOUSE_CPP_DIR/contrib/absl])

  dnl Static clickhouse-cpp + its bundled contrib (cityhash, lz4, zstd, absl),
  dnl built via CMake into third_party/clickhouse-cpp/build (see docs/installation.md).
  dnl --start-group/--end-group resolves the inter-archive symbol order so we
  dnl don't have to hand-sort the five archives.
  CLICKHOUSE_CPP_BUILD="$CLICKHOUSE_CPP_DIR/build"
  AC_MSG_CHECKING([for the prebuilt clickhouse-cpp static library])
  if test ! -f "$CLICKHOUSE_CPP_BUILD/clickhouse/libclickhouse-cpp-lib.a"; then
    AC_MSG_ERROR([clickhouse-cpp is not built. Run the CMake build first:
  cmake -S third_party/clickhouse-cpp -B third_party/clickhouse-cpp/build \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DBUILD_TESTS=OFF -DBUILD_BENCHMARK=OFF
  cmake --build third_party/clickhouse-cpp/build -j])
  fi
  AC_MSG_RESULT([found])

  CLICKHOUSE_ASYNC_SHARED_LIBADD="$CLICKHOUSE_ASYNC_SHARED_LIBADD -Wl,--start-group \
$CLICKHOUSE_CPP_BUILD/clickhouse/libclickhouse-cpp-lib.a \
$CLICKHOUSE_CPP_BUILD/contrib/cityhash/cityhash/libcityhash.a \
$CLICKHOUSE_CPP_BUILD/contrib/lz4/lz4/liblz4.a \
$CLICKHOUSE_CPP_BUILD/contrib/zstd/zstd/libzstdstatic.a \
$CLICKHOUSE_CPP_BUILD/contrib/absl/absl/libabsl_int128.a \
-Wl,--end-group"

  PHP_SUBST([CLICKHOUSE_ASYNC_SHARED_LIBADD])

  PHP_NEW_EXTENSION([clickhouse_async],
    [clickhouse_async.cpp ch_transport.cpp ch_exceptions.cpp ch_client.cpp],
    [$ext_shared],,
    [$CLICKHOUSE_ASYNC_STD],
    [cxx])
fi
