#include "socket.hpp"

#include <spdlog/spdlog.h>

void socket_t::erase(const std::uint64_t size)
{
	auto dummy_buffer = std::vector<std::uint8_t>(size);

	read(dummy_buffer.data(), size);
}

void socket_t::async_erase(const std::uint64_t size, const async_callback_t& handler)
{
	const auto dummy_buffer = std::make_shared<std::vector<std::uint8_t>>(size);

	async_read(dummy_buffer->data(), size,
		[this, handler, size](const std::uint8_t is_valid)
		{
			if (!is_valid)
			{
				spdlog::error("failed to erase {} bytes from socket", size);
			}

			handler(is_valid);
		}
	);
}

std::uint8_t boost_tcp_socket_t::connect(const std::string_view& host, const std::string_view& service)
{
	const std::optional<resolver_t::results_type> endpoints = resolve_host(host, service);

	if (!endpoints.has_value())
	{
		return 0;
	}

	boost::system::error_code error_code = { };

	boost::asio::connect(stream_->lowest_layer(), *endpoints, error_code);

	return !error_code.failed();
}

std::uint8_t boost_tcp_socket_t::connect(const std::uint32_t ipv4_address, const std::uint16_t port)
{
	const boost::asio::ip::address_v4 address(ipv4_address);
	const boost::asio::ip::tcp::endpoint endpoint(address, port);

	boost::system::error_code error_code = { };

	auto& lowest_layer = stream_->lowest_layer();

	lowest_layer.connect(endpoint, error_code);

	return !error_code.failed();
}

void boost_tcp_socket_t::close()
{
	//stream_->shutdown();

	auto& lowest_layer = stream_->lowest_layer();

	lowest_layer.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
	lowest_layer.close();
}

std::uint8_t boost_tcp_socket_t::handshake(const handshake_type_t type)
{
	boost::system::error_code error_code = { };

	const asio_handshake_type_t asio_type = asio_handshake_type(type);

	stream_->handshake(asio_type, error_code);

	return !error_code.failed();
}

void boost_tcp_socket_t::async_handshake(const handshake_type_t type, const async_callback_t& handler)
{
	const asio_handshake_type_t asio_type = asio_handshake_type(type);

	stream_->async_handshake(asio_type,
		[handler](const boost::system::error_code& error_code)
		{
			const std::uint8_t is_valid = !error_code;

			if (!is_valid)
			{
				spdlog::error(error_code.what());
			}

			handler(is_valid);
		}
	);
}

std::uint8_t boost_tcp_socket_t::read(void* const buffer, const std::uint64_t size)
{
	boost::system::error_code error_code = { };

	boost::asio::read(*stream_, boost::asio::buffer(buffer, size), error_code);

	return !error_code;
}

void boost_tcp_socket_t::async_read(void* const buffer, const std::uint64_t size, const async_callback_t& handler)
{
	boost::asio::async_read(*stream_, boost::asio::buffer(buffer, size),
		[handler](const boost::system::error_code& error_code, const std::uint64_t)
		{
			const std::uint8_t is_valid = !error_code;

			if (!is_valid)
			{
				spdlog::error(error_code.what());
			}

			handler(is_valid);
		}
	);
}

std::uint8_t boost_tcp_socket_t::write(const void* const buffer, const std::uint64_t size)
{
	boost::system::error_code error_code = { };

	boost::asio::write(*stream_, boost::asio::buffer(buffer, size), error_code);

	return !error_code;
}

void boost_tcp_socket_t::async_write(const void* const buffer, const std::uint64_t size, const async_callback_t& handler)
{
	boost::asio::async_write(*stream_, boost::asio::buffer(buffer, size),
		[handler](const boost::system::error_code& error_code, const std::uint64_t)
		{
			const std::uint8_t is_valid = !error_code;

			if (!is_valid)
			{
				spdlog::error(error_code.what());
			}

			handler(is_valid);
		}
	);
}

std::uint32_t boost_tcp_socket_t::ipv4_address()
{
	const asio_endpoint_t remote_endpoint_ = remote_endpoint();
	const auto address = remote_endpoint_.address();
	const auto ipv4_address = address.to_v4();

	return ipv4_address.to_uint();
}

std::uint16_t boost_tcp_socket_t::port()
{
	const asio_endpoint_t endpoint_ = local_endpoint();

	return endpoint_.port();
}

std::optional<boost_tcp_socket_t::resolver_t::results_type> boost_tcp_socket_t::resolve_host(const std::string_view& host, const std::string_view& service) const
{
	boost::system::error_code error_code = { };

	resolver_t resolver(*io_context_);
	resolver_t::results_type endpoints = resolver.resolve(host, service);

	if (error_code)
	{
		return std::nullopt;
	}

	return endpoints;
}

boost_tcp_socket_t::asio_endpoint_t boost_tcp_socket_t::remote_endpoint() const
{
	const auto& lowest_layer = stream_->lowest_layer();

	return lowest_layer.remote_endpoint();
}

boost_tcp_socket_t::asio_endpoint_t boost_tcp_socket_t::local_endpoint() const
{
	const auto& lowest_layer = stream_->lowest_layer();

	return lowest_layer.local_endpoint();
}

boost_tcp_socket_t::asio_handshake_type_t boost_tcp_socket_t::asio_handshake_type(handshake_type_t type)
{
	return type == handshake_type_t::client ? asio_handshake_type_t::client : asio_handshake_type_t::server;
}
