/*
  +----------------------------------------------------------------------+
  | php-clickhouse — native async ClickHouse client for PHP TrueAsync    |
  +----------------------------------------------------------------------+
  | Licensed under the Apache License, Version 2.0 (the "License").      |
  +----------------------------------------------------------------------+

  Transport bridge between clickhouse-cpp and the TrueAsync reactor.

  clickhouse-cpp owns the protocol and buffers/compresses on top of the
  transport; the InputStream::DoRead / OutputStream::DoWrite we provide are the
  bottom layer and move raw bytes. We reuse clickhouse-cpp's own
  connect/DNS/socket-option logic (NonSecureSocketFactory + the protected
  Socket::handle_) and replace only the byte movement with reactor-driven,
  non-blocking recv/send.

  The readiness wait follows the canonical TrueAsync DB-driver pattern (see
  ext/pdo_pgsql/pgsql_driver.c pdo_pgsql_await_socket): a single reactor poll
  event + waker C callback + one suspend. No per-read request allocation and no
  re-arming suspend loop.

  NOTE: the initial TCP connect is still blocking (clickhouse-cpp's Socket ctor);
  async connect is a follow-up. The read/write path is reactor-driven.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

extern "C" {
#include "php.h"
#include "Zend/zend_async_API.h"
#include "main/php_streams.h"
}

#include <clickhouse/base/socket.h>
#include <clickhouse/client.h>

#include <errno.h>

#ifdef PHP_WIN32
/* winsock2.h / ws2tcpip.h are pulled in by <clickhouse/base/socket.h> above. */
#else
#include <fcntl.h>
#include <sys/socket.h>
#endif

#include <memory>

#include "ch_exceptions.h"
#include "ch_transport.h"

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

/* The byte-movement paths below are identical across platforms except for how
 * a socket reports "would block": POSIX sets errno (EAGAIN/EWOULDBLOCK), Winsock
 * keeps its own WSAE* codes behind WSAGetLastError(), and recv/send take an int
 * length on Windows. These shims absorb that difference. */
#ifdef PHP_WIN32
#define CH_SOCK_LAST_ERROR   WSAGetLastError()
#define CH_SOCK_EINTR        WSAEINTR
#define CH_SOCK_EAGAIN       WSAEWOULDBLOCK
#define CH_SOCK_EWOULDBLOCK  WSAEWOULDBLOCK
#define CH_SOCK_IOLEN(len)   (static_cast<int>(len))
#else
#define CH_SOCK_LAST_ERROR   errno
#define CH_SOCK_EINTR        EINTR
#define CH_SOCK_EAGAIN       EAGAIN
#define CH_SOCK_EWOULDBLOCK  EWOULDBLOCK
#define CH_SOCK_IOLEN(len)   (len)
#endif

namespace {

/*
 * Suspend the current coroutine until `fd` is ready for `events`. Returns true
 * on readiness; false on cancellation / timeout / error (a PHP exception may be
 * pending). Mirrors pdo_pgsql_await_socket.
 */
bool ch_await_socket(zend_socket_t fd, async_poll_event events, zend_ulong timeout_ms)
{
	zend_coroutine_t *coroutine = ZEND_ASYNC_CURRENT_COROUTINE;

	if (coroutine == nullptr) {
		return false;
	}

	zend_async_poll_event_t *poll_event = ZEND_ASYNC_NEW_SOCKET_EVENT(fd, events);

	if (poll_event == nullptr || EG(exception) != nullptr) {
		return false;
	}

	zend_async_waker_new_with_timeout(coroutine, timeout_ms, NULL);

	if (EG(exception) != nullptr) {
		ZEND_ASYNC_EVENT_RELEASE(&poll_event->base);
		return false;
	}

	zend_async_resume_when(coroutine, &poll_event->base, true,
			zend_async_waker_callback_resolve, NULL);

	if (EG(exception) != nullptr) {
		zend_async_waker_clean(coroutine);
		return false;
	}

	bool suspended = ZEND_ASYNC_SUSPEND();

	zend_async_waker_clean(coroutine);

	return suspended && EG(exception) == nullptr;
}

/* InputStream: non-blocking recv, awaiting readability through the reactor. */
class AsyncInput final : public clickhouse::InputStream {
public:
	explicit AsyncInput(zend_socket_t fd) : fd_(fd) {}

protected:
	bool Skip(size_t /*bytes*/) override { return false; }

	size_t DoRead(void *buf, size_t len) override
	{
		for (;;) {
			ssize_t n = ::recv(fd_, static_cast<char *>(buf), CH_SOCK_IOLEN(len), 0);

			if (n > 0) {
				return static_cast<size_t>(n);
			}

			if (n == 0) {
				throw chasync::ConnectionError("clickhouse: connection closed by peer");
			}

			if (CH_SOCK_LAST_ERROR == CH_SOCK_EINTR) {
				continue;
			}

			if (CH_SOCK_LAST_ERROR == CH_SOCK_EAGAIN || CH_SOCK_LAST_ERROR == CH_SOCK_EWOULDBLOCK) {
				if (!ch_await_socket(fd_, ASYNC_READABLE, 0)) {
					throw chasync::ConnectionError("clickhouse: read interrupted");
				}

				continue;
			}

			throw chasync::ConnectionError("clickhouse: recv failed");
		}
	}

private:
	zend_socket_t fd_;  /* owned by the clickhouse-cpp Socket */
};

/* OutputStream: non-blocking send, awaiting writability through the reactor. */
class AsyncOutput final : public clickhouse::OutputStream {
public:
	explicit AsyncOutput(zend_socket_t fd) : fd_(fd) {}

protected:
	size_t DoWrite(const void *data, size_t len) override
	{
		const char *p = static_cast<const char *>(data);
		size_t sent = 0;

		while (sent < len) {
			ssize_t n = ::send(fd_, p + sent, CH_SOCK_IOLEN(len - sent), MSG_NOSIGNAL);

			if (n > 0) {
				sent += static_cast<size_t>(n);
				continue;
			}

			if (CH_SOCK_LAST_ERROR == CH_SOCK_EINTR) {
				continue;
			}

			if (CH_SOCK_LAST_ERROR == CH_SOCK_EAGAIN || CH_SOCK_LAST_ERROR == CH_SOCK_EWOULDBLOCK) {
				if (!ch_await_socket(fd_, ASYNC_WRITABLE, 0)) {
					throw chasync::ConnectionError("clickhouse: write interrupted");
				}

				continue;
			}

			throw chasync::ConnectionError("clickhouse: send failed");
		}

		return len;
	}

private:
	zend_socket_t fd_;  /* owned by the clickhouse-cpp Socket */
};

/*
 * clickhouse-cpp Socket whose byte movement goes through the reactor. The base
 * ctor connects (blocking for now) and sets the protected handle_; we only
 * switch it to non-blocking. The fd stays owned by the base Socket (closed in
 * ~Socket), so there is no fd-ownership handoff to coordinate.
 */
class AsyncSocket final : public clickhouse::Socket {
public:
	explicit AsyncSocket(const clickhouse::NetworkAddress &addr)
		: clickhouse::Socket(addr)
	{
		set_nonblocking();
	}

	AsyncSocket(const clickhouse::NetworkAddress &addr,
			const clickhouse::SocketTimeoutParams &timeout_params)
		: clickhouse::Socket(addr, timeout_params)
	{
		set_nonblocking();
	}

	std::unique_ptr<clickhouse::InputStream> makeInputStream() const override
	{
		return std::make_unique<AsyncInput>(handle_);
	}

	std::unique_ptr<clickhouse::OutputStream> makeOutputStream() const override
	{
		return std::make_unique<AsyncOutput>(handle_);
	}

private:
	void set_nonblocking()
	{
#ifdef PHP_WIN32
		u_long nonblocking = 1;
		ioctlsocket(handle_, FIONBIO, &nonblocking);
#else
		int flags = fcntl(handle_, F_GETFL, 0);
		if (flags >= 0) {
			fcntl(handle_, F_SETFL, flags | O_NONBLOCK);
		}
#endif
	}
};

/* TLS transport: a php_stream ssl:// socket. php_stream socket IO is async
 * under TrueAsync, so the handshake and reads/writes yield the coroutine. */
class TlsInput final : public clickhouse::InputStream {
public:
	explicit TlsInput(php_stream *stream) : stream_(stream) {}

protected:
	bool Skip(size_t /*bytes*/) override { return false; }

	size_t DoRead(void *buf, size_t len) override
	{
		ssize_t n = php_stream_read(stream_, static_cast<char *>(buf), len);

		if (n <= 0) {
			throw chasync::ConnectionError("clickhouse: tls connection closed");
		}

		return static_cast<size_t>(n);
	}

private:
	php_stream *stream_;  /* owned by TlsSocket */
};

class TlsOutput final : public clickhouse::OutputStream {
public:
	explicit TlsOutput(php_stream *stream) : stream_(stream) {}

protected:
	size_t DoWrite(const void *data, size_t len) override
	{
		const char *p = static_cast<const char *>(data);
		size_t sent = 0;

		while (sent < len) {
			ssize_t n = php_stream_write(stream_, p + sent, len - sent);

			if (n <= 0) {
				throw chasync::ConnectionError("clickhouse: tls write failed");
			}

			sent += static_cast<size_t>(n);
		}

		return len;
	}

private:
	php_stream *stream_;  /* owned by TlsSocket */
};

class TlsSocket final : public clickhouse::SocketBase {
public:
	TlsSocket(const std::string &host, uint16_t port, bool verify)
	{
		php_stream_context *ctx = php_stream_context_alloc();
		zval flag;
		ZVAL_BOOL(&flag, verify);
		php_stream_context_set_option(ctx, "ssl", "verify_peer", &flag);
		php_stream_context_set_option(ctx, "ssl", "verify_peer_name", &flag);

		std::string url = "ssl://" + host + ":" + std::to_string(port);
		zend_string *errstr = nullptr;
		int errcode = 0;

		stream_ = php_stream_xport_create(url.c_str(), url.size(), 0,
				STREAM_XPORT_CLIENT | STREAM_XPORT_CONNECT, nullptr, nullptr, ctx,
				&errstr, &errcode);

		std::string error = errstr != nullptr
			? std::string(ZSTR_VAL(errstr), ZSTR_LEN(errstr))
			: "tls connect failed";

		if (errstr != nullptr) {
			zend_string_release(errstr);
		}

		if (stream_ == nullptr) {
			throw chasync::ConnectionError("clickhouse: " + error);
		}
	}

	~TlsSocket() override
	{
		if (stream_ != nullptr) {
			php_stream_close(stream_);
		}
	}

	std::unique_ptr<clickhouse::InputStream> makeInputStream() const override
	{
		return std::make_unique<TlsInput>(stream_);
	}

	std::unique_ptr<clickhouse::OutputStream> makeOutputStream() const override
	{
		return std::make_unique<TlsOutput>(stream_);
	}

private:
	php_stream *stream_;
};

/* Factory: reuse NonSecureSocketFactory connect()/DNS/socket-options for the
 * plaintext path; for TLS, connect a php_stream ssl:// socket instead. */
class AsyncSocketFactory final : public clickhouse::NonSecureSocketFactory {
public:
	AsyncSocketFactory(bool tls, bool verify) : tls_(tls), verify_(verify) {}

	std::unique_ptr<clickhouse::SocketBase> connect(const clickhouse::ClientOptions &opts,
			const clickhouse::Endpoint &endpoint) override
	{
		if (tls_) {
			return std::make_unique<TlsSocket>(endpoint.host, endpoint.port, verify_);
		}

		return clickhouse::NonSecureSocketFactory::connect(opts, endpoint);
	}

protected:
	std::unique_ptr<clickhouse::Socket> doConnect(const clickhouse::NetworkAddress &address,
			const clickhouse::ClientOptions & /*opts*/) override
	{
		return std::make_unique<AsyncSocket>(address);
	}

private:
	bool tls_;
	bool verify_;
};

} // namespace

std::unique_ptr<clickhouse::SocketFactory> ch_make_async_socket_factory(bool tls, bool verify)
{
	return std::make_unique<AsyncSocketFactory>(tls, verify);
}
