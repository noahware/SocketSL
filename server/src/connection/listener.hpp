#pragma once
#include "connection.hpp"

#include <spdlog/spdlog.h>

class connection_listener_t
{
public:
	connection_listener_t() = default;
	virtual ~connection_listener_t() = default;

	virtual void async_wait_for_connection() = 0;

	void add_connection(std::shared_ptr<connection_t> connection);
	void remove_connection(connection_t* connection);

protected:
	std::vector<std::shared_ptr<connection_t>> connections_;
};

// must be created as a shared ptr
template <class connection_type_t>
class boost_connection_listener_t final : public connection_listener_t, public std::enable_shared_from_this<boost_connection_listener_t<connection_type_t>>
{
	static_assert(std::is_base_of_v<connection_t, connection_type_t>, "connection_type_t must derive from connection_t");

public:
	typedef boost::asio::ip::tcp tcp_t;
	typedef boost::asio::io_context asio_context_t;
	typedef boost::asio::ip::tcp::acceptor acceptor_t;
	typedef boost::asio::ip::tcp::endpoint endpoint_t;
	typedef boost::asio::ip::tcp::socket asio_socket_t;

	boost_connection_listener_t(std::shared_ptr<asio_context_t> io_context, std::shared_ptr<boost_ssl_context_t> ssl_context, const std::uint16_t port)
			:	io_context_(std::move(io_context)),
				ssl_context_(std::move(ssl_context)),
				acceptor_(std::make_unique<acceptor_t>(*io_context_, endpoint_t(tcp_t::v4(), port))) { }

	void async_wait_for_connection() override;

protected:
	std::shared_ptr<asio_context_t> io_context_;
	std::shared_ptr<boost_ssl_context_t> ssl_context_;
	std::unique_ptr<acceptor_t> acceptor_;
};

template <class connection_type_t>
void boost_connection_listener_t<connection_type_t>::async_wait_for_connection()
{
	acceptor_->async_accept(
		[this](const boost::system::error_code& error_code, asio_socket_t asio_socket)
		{
			if (!error_code)
			{
				const auto local_endpoint = asio_socket.local_endpoint();

				const auto remote_endpoint = asio_socket.remote_endpoint();
				const auto endpoint_address = remote_endpoint.address();

				spdlog::info("accepting connection from {} on port {}", endpoint_address.to_string(), local_endpoint.port());

				auto socket = std::make_unique<boost_tcp_socket_t>(io_context_, std::move(asio_socket), ssl_context_);
				auto connection = std::make_shared<connection_type_t>(std::move(socket), this->shared_from_this());

				add_connection(std::move(connection));
			}
			else
			{
				spdlog::error(error_code.what());
			}

			async_wait_for_connection();
		}
	);
}
