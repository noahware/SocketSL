#include "socket.hpp"

#include <log/log.hpp>

namespace sl
{
	void socket::erase(const std::size_t size)
	{
		auto dummy_buffer = std::vector<std::uint8_t>(size);

		(void)read(dummy_buffer);
	}

	void socket::async_erase(const std::size_t size, const async_callback_t& handler)
	{
		const auto dummy_buffer = std::make_shared<std::vector<std::uint8_t>>(size);

		async_read(*dummy_buffer,
			[handler, size, dummy_buffer](const bool is_valid)
			{
				if (!is_valid)
				{
					LOG_ERR("failed to erase {} bytes from socket", size);
				}

				handler(is_valid);
			}
		);
	}

	bool boost_tcp_socket::connect(const std::string_view host, const std::string_view service)
	{
		const std::optional<resolver_type::results_type> endpoints = resolve_host(host, service);

		if (!endpoints.has_value())
		{
			return false;
		}

		boost::system::error_code error_code = { };

		boost::asio::connect(stream_->lowest_layer(), *endpoints, error_code);

		return !error_code.failed();
	}

	bool boost_tcp_socket::connect(const std::uint32_t ipv4_address, const std::uint16_t port)
	{
		const boost::asio::ip::address_v4 address(ipv4_address);
		const boost::asio::ip::tcp::endpoint endpoint(address, port);

		boost::system::error_code error_code = { };

		auto& lowest_layer = stream_->lowest_layer();

		lowest_layer.connect(endpoint, error_code);

		return !error_code.failed();
	}

	void boost_tcp_socket::close()
	{
		auto& lowest_layer = stream_->lowest_layer();

		lowest_layer.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
		lowest_layer.close();
	}

	bool boost_tcp_socket::handshake(const handshake_type type)
	{
		boost::system::error_code error_code = { };

		const asio_handshake_type asio_type = to_asio_handshake_type(type);

		stream_->handshake(asio_type, error_code);

		return !error_code.failed();
	}

	void boost_tcp_socket::async_handshake(const handshake_type type, const async_callback_t& handler)
	{
		const asio_handshake_type asio_type = to_asio_handshake_type(type);

		stream_->async_handshake(asio_type,
			[handler](const boost::system::error_code& error_code)
			{
				const bool is_valid = !error_code;

				if (!is_valid)
				{
					LOG_ERR(error_code.what());
				}

				handler(is_valid);
			}
		);
	}

	bool boost_tcp_socket::read(const std::span<std::uint8_t> buffer)
	{
		boost::system::error_code error_code = { };

		boost::asio::read(*stream_, boost::asio::buffer(buffer.data(), buffer.size()), error_code);

		return !error_code;
	}

	void boost_tcp_socket::async_read(const std::span<std::uint8_t> buffer, const async_callback_t& handler)
	{
		boost::asio::async_read(*stream_, boost::asio::buffer(buffer.data(), buffer.size()),
			[handler](const boost::system::error_code& error_code, const std::size_t)
			{
				const bool is_valid = !error_code;

				if (!is_valid)
				{
					LOG_ERR(error_code.what());
				}

				handler(is_valid);
			}
		);
	}

	bool boost_tcp_socket::write(const std::span<const std::uint8_t> buffer)
	{
		boost::system::error_code error_code = { };

		boost::asio::write(*stream_, boost::asio::buffer(buffer.data(), buffer.size()), error_code);

		return !error_code;
	}

	void boost_tcp_socket::async_write(const std::span<const std::uint8_t> buffer, const async_callback_t& handler)
	{
		boost::asio::async_write(*stream_, boost::asio::buffer(buffer.data(), buffer.size()),
			[handler](const boost::system::error_code& error_code, const std::size_t)
			{
				const bool is_valid = !error_code;

				if (!is_valid)
				{
					LOG_ERR(error_code.what());
				}

				handler(is_valid);
			}
		);
	}

	std::uint32_t boost_tcp_socket::ipv4_address() const
	{
		const asio_endpoint_type remote = remote_endpoint();
		const auto address = remote.address();
		const auto ipv4 = address.to_v4();

		return ipv4.to_uint();
	}

	std::uint16_t boost_tcp_socket::port() const
	{
		const asio_endpoint_type endpoint = local_endpoint();

		return endpoint.port();
	}

	std::optional<boost_tcp_socket::resolver_type::results_type> boost_tcp_socket::resolve_host(const std::string_view host, const std::string_view service) const
	{
		boost::system::error_code error_code = { };

		resolver_type resolver(*io_context_);
		resolver_type::results_type endpoints = resolver.resolve(host, service);

		if (error_code)
		{
			return std::nullopt;
		}

		return endpoints;
	}

	boost_tcp_socket::asio_endpoint_type boost_tcp_socket::remote_endpoint() const
	{
		const auto& lowest_layer = stream_->lowest_layer();

		return lowest_layer.remote_endpoint();
	}

	boost_tcp_socket::asio_endpoint_type boost_tcp_socket::local_endpoint() const
	{
		const auto& lowest_layer = stream_->lowest_layer();

		return lowest_layer.local_endpoint();
	}

	boost_tcp_socket::asio_handshake_type boost_tcp_socket::to_asio_handshake_type(const handshake_type type) noexcept
	{
		return type == handshake_type::client ? asio_handshake_type::client : asio_handshake_type::server;
	}
}
