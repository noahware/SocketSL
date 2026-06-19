#include "socket.hpp"

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
			[handler, dummy_buffer](const bool is_valid)
			{
				handler(is_valid);
			}
		);
	}

	void boost_tcp_socket::set_timeout(const timeout_duration timeout)
	{
		timeout_ = timeout;
	}

	void boost_tcp_socket::set_max_message_size(const std::size_t max_size)
	{
		max_message_size_ = max_size;
	}

	std::size_t boost_tcp_socket::max_message_size() const
	{
		return max_message_size_;
	}

	void boost_tcp_socket::start_deadline()
	{
		if (timeout_ == timeout_duration::zero())
		{
			return;
		}

		if (!timer_)
		{
			timer_ = std::make_unique<boost::asio::steady_timer>(executor_);
		}

		timer_->expires_after(timeout_);
		timer_->async_wait(
			[this](const boost::system::error_code& ec)
			{
				if (!ec)
				{
					stream_->lowest_layer().close();
				}
			}
		);
	}

	void boost_tcp_socket::cancel_deadline()
	{
		if (timer_)
		{
			timer_->cancel();
		}
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

	void boost_tcp_socket::async_connect(const std::string_view host, const std::string_view service, const async_callback_t& handler)
	{
		start_deadline();

		const auto resolver = std::make_shared<resolver_type>(executor_);

		resolver->async_resolve(host, service,
			[this, handler, resolver](const boost::system::error_code& error_code, const resolver_type::results_type& endpoints)
			{
				if (error_code)
				{
					cancel_deadline();

					handler(false);

					return;
				}

				boost::asio::async_connect(stream_->lowest_layer(), endpoints,
					[this, handler](const boost::system::error_code& connect_error_code, const asio_endpoint_type& endpoint)
					{
						cancel_deadline();

						if (!connect_error_code)
						{
							remote_endpoint_ = endpoint;
						}

						handler(!connect_error_code);
					}
				);
			}
		);
	}

	void boost_tcp_socket::close()
	{
		auto& lowest_layer = stream_->lowest_layer();

		boost::system::error_code error_code = { };

		lowest_layer.shutdown(boost::asio::ip::tcp::socket::shutdown_both, error_code);
		lowest_layer.close(error_code);
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
		start_deadline();

		const asio_handshake_type asio_type = to_asio_handshake_type(type);

		stream_->async_handshake(asio_type,
			[this, handler](const boost::system::error_code& error_code)
			{
				cancel_deadline();

				handler(!error_code);
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
		start_deadline();

		boost::asio::async_read(*stream_, boost::asio::buffer(buffer.data(), buffer.size()),
			[this, handler](const boost::system::error_code& error_code, const std::size_t)
			{
				cancel_deadline();

				handler(!error_code);
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
		start_deadline();

		boost::asio::async_write(*stream_, boost::asio::buffer(buffer.data(), buffer.size()),
			[this, handler](const boost::system::error_code& error_code, const std::size_t)
			{
				cancel_deadline();

				handler(!error_code);
			}
		);
	}

	std::uint32_t boost_tcp_socket::ipv4_address() const
	{
		return remote_endpoint_.address().to_v4().to_uint();
	}

	std::uint16_t boost_tcp_socket::port() const
	{
		return remote_endpoint_.port();
	}

	std::string boost_tcp_socket::remote_address() const
	{
		return remote_endpoint_.address().to_string();
	}

	std::optional<boost_tcp_socket::resolver_type::results_type> boost_tcp_socket::resolve_host(const std::string_view host, const std::string_view service) const
	{
		boost::system::error_code error_code = { };

		resolver_type resolver(executor_);
		resolver_type::results_type endpoints = resolver.resolve(host, service);

		if (error_code)
		{
			return std::nullopt;
		}

		return endpoints;
	}

	void boost_tcp_socket::capture_remote_endpoint()
	{
		boost::system::error_code error_code = { };

		const asio_endpoint_type endpoint = stream_->lowest_layer().remote_endpoint(error_code);

		if (!error_code)
		{
			remote_endpoint_ = endpoint;
		}
	}

	boost_tcp_socket::asio_handshake_type boost_tcp_socket::to_asio_handshake_type(const handshake_type type) noexcept
	{
		return type == handshake_type::client ? asio_handshake_type::client : asio_handshake_type::server;
	}
}
