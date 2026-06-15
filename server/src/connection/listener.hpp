#pragma once
#include "connection.hpp"

#include <spdlog/spdlog.h>

namespace sl
{
	class connection_listener
	{
	public:
		connection_listener() = default;
		virtual ~connection_listener() = default;

		virtual void async_wait_for_connection() = 0;

		void add_connection(std::shared_ptr<connection> connection);
		void remove_connection(connection* connection);

	protected:
		std::vector<std::shared_ptr<connection>> connections_;
	};

	// must be created as a shared ptr
	template <class ConnectionT>
	class boost_connection_listener final : public connection_listener, public std::enable_shared_from_this<boost_connection_listener<ConnectionT>>
	{
		static_assert(std::is_base_of_v<connection, ConnectionT>, "ConnectionT must derive from connection");

	public:
		using tcp_type = boost::asio::ip::tcp;
		using asio_context_type = boost::asio::io_context;
		using acceptor_type = boost::asio::ip::tcp::acceptor;
		using endpoint_type = boost::asio::ip::tcp::endpoint;
		using asio_socket_type = boost::asio::ip::tcp::socket;

		boost_connection_listener(std::shared_ptr<asio_context_type> io_context, std::shared_ptr<boost_ssl_context> ssl_context, const std::uint16_t port)
				:	io_context_(std::move(io_context)),
					ssl_context_(std::move(ssl_context)),
					acceptor_(std::make_unique<acceptor_type>(*io_context_, endpoint_type(tcp_type::v4(), port))) { }

		void async_wait_for_connection() override;

	protected:
		std::shared_ptr<asio_context_type> io_context_;
		std::shared_ptr<boost_ssl_context> ssl_context_;
		std::unique_ptr<acceptor_type> acceptor_;
	};

	template <class ConnectionT>
	void boost_connection_listener<ConnectionT>::async_wait_for_connection()
	{
		acceptor_->async_accept(
			[this](const boost::system::error_code& error_code, asio_socket_type asio_socket)
			{
				if (!error_code)
				{
					const auto local_endpoint = asio_socket.local_endpoint();

					const auto remote_endpoint = asio_socket.remote_endpoint();
					const auto endpoint_address = remote_endpoint.address();

					spdlog::info("accepting connection from {} on port {}", endpoint_address.to_string(), local_endpoint.port());

					auto socket = std::make_unique<boost_tcp_socket>(io_context_, std::move(asio_socket), ssl_context_);
					auto connection = std::make_shared<ConnectionT>(std::move(socket), this->shared_from_this());

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
}
