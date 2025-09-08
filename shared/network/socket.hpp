#pragma once
#include <boost/asio.hpp>

#include "ssl.hpp"

typedef std::function<void(std::uint8_t is_valid)> async_callback_t;

class socket_t
{
public:
	enum class handshake_type_t : std::uint8_t
	{
		client,
		server
	};

	socket_t() = default;
	virtual ~socket_t() = default;

	virtual std::uint8_t connect(const std::string_view& host, const std::string_view& service) = 0;
	virtual std::uint8_t connect(std::uint32_t ipv4_address, std::uint16_t port) = 0;
	virtual void close() = 0;

	virtual std::uint8_t handshake(handshake_type_t type) = 0;
	virtual void async_handshake(handshake_type_t type, const async_callback_t& handler) = 0;

	void erase(std::uint64_t size);
	void async_erase(std::uint64_t size, const async_callback_t& handler);

	virtual std::uint8_t read(void* buffer, std::uint64_t size) = 0;
	virtual void async_read(void* buffer, std::uint64_t size, const async_callback_t& handler) = 0;

	virtual std::uint8_t write(const void* buffer, std::uint64_t size) = 0;
	virtual void async_write(const void* buffer, std::uint64_t size, const async_callback_t& handler) = 0;

	[[nodiscard]] virtual std::uint32_t ipv4_address() = 0;
	[[nodiscard]] virtual std::uint16_t port() = 0;

	template <class t>
	std::uint8_t read(t& value)
	{
		const std::uint8_t status = this->read(&value, sizeof(value));

		return status;
	}

	template <class t>
	void async_read(t& value, const async_callback_t& handler)
	{
		this->async_read(&value, sizeof(value), handler);
	}

	template <class t>
	std::uint8_t write(t value)
	{
		const std::uint8_t status = this->write(&value, sizeof(value));

		return status;
	}

	template <class t>
	void async_write(t value, const async_callback_t& handler)
	{
		this->async_write(&value, sizeof(value), handler);
	}
};

class boost_tcp_socket_t final : public socket_t
{
public:
	typedef boost::asio::io_context asio_context_t;
	typedef boost::asio::ip::tcp::resolver resolver_t;
	typedef boost::asio::ip::tcp::socket asio_socket_t;
	typedef boost::asio::ssl::stream<asio_socket_t> asio_stream_t;
	typedef boost::asio::ssl::stream_base::handshake_type asio_handshake_type_t;
	typedef boost::asio::ssl::stream_base::handshake_type asio_handshake_type_t;
	typedef asio_socket_t::endpoint_type asio_endpoint_t;

	explicit boost_tcp_socket_t(std::shared_ptr<asio_context_t> io_context, std::shared_ptr<boost_ssl_context_t> ssl_context)
		:	io_context_(std::move(io_context)),
			ssl_context_(std::move(ssl_context)),
			stream_(std::make_unique<asio_stream_t>(*io_context_, ssl_context_->native_handle())) {}

	explicit boost_tcp_socket_t(std::shared_ptr<asio_context_t> io_context, asio_socket_t socket, std::shared_ptr<boost_ssl_context_t> ssl_context)
		:	io_context_(std::move(io_context)),
			ssl_context_(std::move(ssl_context)),
			stream_(std::make_unique<asio_stream_t>(std::move(socket), ssl_context_->native_handle())) {}

	std::uint8_t connect(const std::string_view& host, const std::string_view& service) override;
	std::uint8_t connect(std::uint32_t ipv4_address, std::uint16_t port) override;

	void close() override;

	std::uint8_t handshake(handshake_type_t type) override;
	void async_handshake(handshake_type_t type, const async_callback_t& handler) override;

	std::uint8_t read(void* buffer, std::uint64_t size) override;
	void async_read(void* buffer, std::uint64_t size, const async_callback_t& handler) override;

	std::uint8_t write(const void* buffer, std::uint64_t size) override;
	void async_write(const void* buffer, std::uint64_t size, const async_callback_t& handler) override;

	[[nodiscard]] std::uint32_t ipv4_address() override;
	[[nodiscard]] std::uint16_t port() override;

protected:
	[[nodiscard]] std::optional<resolver_t::results_type> resolve_host(const std::string_view& host, const std::string_view& service) const;
	[[nodiscard]] asio_endpoint_t remote_endpoint() const;
	[[nodiscard]] asio_endpoint_t local_endpoint() const;

	static asio_handshake_type_t asio_handshake_type(handshake_type_t type);

	std::shared_ptr<asio_context_t> io_context_;
	std::shared_ptr<boost_ssl_context_t> ssl_context_;
	std::unique_ptr<asio_stream_t> stream_;
};

