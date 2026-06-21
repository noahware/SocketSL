#include "socket.hpp"
#include <message/message.hpp>

#include <schema/system_generated.h>

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

	void boost_tcp_socket::set_idle_timeout(const timeout_duration timeout)
	{
		idle_timeout_ = timeout;
	}

	void boost_tcp_socket::set_heartbeat_timeout(const timeout_duration timeout)
	{
		heartbeat_timeout_ = timeout;
	}

	void boost_tcp_socket::set_max_message_size(const std::size_t max_size)
	{
		max_message_size_ = max_size;
	}

	std::size_t boost_tcp_socket::max_message_size() const
	{
		return max_message_size_;
	}

	void boost_tcp_socket::set_max_pending_writes(const std::size_t max_pending)
	{
		max_pending_writes_ = max_pending;
	}

	std::size_t boost_tcp_socket::max_pending_writes() const
	{
		return max_pending_writes_;
	}

	void boost_tcp_socket::reset_idle_timer()
	{
		if (idle_timeout_ == timeout_duration::zero())
		{
			return;
		}

		if (!idle_timer_)
		{
			idle_timer_ = std::make_unique<boost::asio::steady_timer>(executor_);
		}

		// expires_after cancels the previous wait (its handler fires with operation_aborted),
		// so each call just restarts the inactivity countdown
		idle_timer_->expires_after(idle_timeout_);
		idle_timer_->async_wait(
			[this](const boost::system::error_code& ec)
			{
				if (!ec)
				{
					stream_->lowest_layer().close();
				}
			}
		);
	}

	void boost_tcp_socket::reset_heartbeat_timer()
	{
		if (heartbeat_timeout_ == timeout_duration::zero())
		{
			return;
		}

		if (!heartbeat_timer_)
		{
			heartbeat_timer_ = std::make_unique<boost::asio::steady_timer>(executor_);
		}

		heartbeat_sent_ = false;

		arm_heartbeat();
	}

	void boost_tcp_socket::arm_heartbeat()
	{
		// expires_after cancels the previous wait (its handler fires with operation_aborted),
		// so each call just restarts the countdown
		heartbeat_timer_->expires_after(heartbeat_timeout_);
		heartbeat_timer_->async_wait(
			[this](const boost::system::error_code& ec)
			{
				if (ec)
				{
					return;
				}

				if (heartbeat_sent_)
				{
					// pinged last interval and still no traffic back -- the peer is gone
					stream_->lowest_layer().close();
				}
				else
				{
					msg::async_send<System::CreateHbPingRequest, true>(*this, System::MessageId_HbPing);

					heartbeat_sent_ = true;

					// wait one more interval for a response; if none arrives we close above
					arm_heartbeat();
				}
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

	void boost_tcp_socket::async_connect(const std::string_view host, const std::string_view service, const async_callback_t& handler)
	{
		reset_idle_timer();
		reset_heartbeat_timer();

		const auto resolver = std::make_shared<resolver_type>(executor_);

		resolver->async_resolve(host, service,
			[this, handler, resolver](const boost::system::error_code& error_code, const resolver_type::results_type& endpoints)
			{
				if (error_code)
				{
					handler(false);

					return;
				}

				boost::asio::async_connect(stream_->lowest_layer(), endpoints,
					[this, handler](const boost::system::error_code& connect_error_code, const asio_endpoint_type& endpoint)
					{
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
		reset_idle_timer();
		reset_heartbeat_timer();

		const asio_handshake_type asio_type = to_asio_handshake_type(type);

		stream_->async_handshake(asio_type,
			[this, handler](const boost::system::error_code& error_code)
			{
				if (!error_code)
				{
					reset_idle_timer();
					reset_heartbeat_timer();
				}

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
		boost::asio::async_read(*stream_, boost::asio::buffer(buffer.data(), buffer.size()),
			[this, handler](const boost::system::error_code& error_code, const std::size_t)
			{
				// a completed read means the peer is alive -- restart the inactivity countdown
				if (!error_code)
				{
					reset_idle_timer();
					reset_heartbeat_timer();
				}

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
		bool idle = false;
		bool rejected = false;

		{
			const std::lock_guard lock(write_mutex_);

			if (max_pending_writes_ != 0 && write_queue_.size() >= max_pending_writes_)
			{
				rejected = true;
			}
			else
			{
				idle = write_queue_.empty();
				write_queue_.push_back({buffer, handler});
			}
		}

		// queue full: the peer isn't draining -- fail this send rather than growing memory unbounded
		if (rejected)
		{
			if (handler)
			{
				handler(false);
			}

			return;
		}

		// only the writer that found the queue idle starts the chain; the rest just queued
		if (idle)
		{
			write_next();
		}
	}

	void boost_tcp_socket::write_next()
	{
		std::span<const std::uint8_t> buffer;

		{
			const std::lock_guard lock(write_mutex_);
			buffer = write_queue_.front().buffer;
		}

		boost::asio::async_write(*stream_, boost::asio::buffer(buffer.data(), buffer.size()),
			[this](const boost::system::error_code& error_code, const std::size_t)
			{
				async_callback_t handler;
				bool more = false;

				{
					const std::lock_guard lock(write_mutex_);

					handler = std::move(write_queue_.front().handler);
					write_queue_.pop_front();
					more = !write_queue_.empty();
				}

				if (handler)
				{
					handler(!error_code);
				}

				if (more)
				{
					write_next();
				}
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
