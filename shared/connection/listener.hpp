#pragma once
#include "server_session.hpp"

#include <log/log.hpp>

#include <mutex>

namespace sl
{
	class server
	{
	public:
		using timeout_duration = socket::timeout_duration;

		server() = default;
		virtual ~server() = default;

		void set_timeout(timeout_duration timeout) noexcept;

		virtual void async_wait_for_connection() = 0;
		virtual void stop();

		void add_connection(std::shared_ptr<server_session> connection);
		void remove_connection(server_session* connection);

		template <class Fn>
		void for_each_conn(Fn&& fn)
		{
			const std::lock_guard lock(connections_mutex_);
			for (const auto& conn : connections_)
			{
				fn(conn);
			}
		}

		[[nodiscard]] std::size_t conn_count() const noexcept;

	protected:
		timeout_duration timeout_{};
		mutable std::mutex connections_mutex_;
		std::vector<std::shared_ptr<server_session>> connections_;
	};

	// must be created as a shared ptr
	template <class ConnectionT>
	class boost_server final : public server, public std::enable_shared_from_this<boost_server<ConnectionT>>
	{
		static_assert(std::is_base_of_v<server_session, ConnectionT>, "ConnectionT must derive from server_session");

	public:
		using executor_type = boost::asio::any_io_executor;
		using tcp_type = boost::asio::ip::tcp;
		using acceptor_type = boost::asio::ip::tcp::acceptor;
		using endpoint_type = boost::asio::ip::tcp::endpoint;
		using asio_socket_type = boost::asio::ip::tcp::socket;

		boost_server(executor_type executor, std::shared_ptr<boost_ssl_context> ssl_context, const std::uint16_t port)
				:	executor_(std::move(executor)),
					ssl_context_(std::move(ssl_context)),
					acceptor_(std::make_unique<acceptor_type>(executor_, endpoint_type(tcp_type::v4(), port))) { }

		void async_wait_for_connection() override;
		void stop() override;

	protected:
		executor_type executor_;
		std::shared_ptr<boost_ssl_context> ssl_context_;
		std::unique_ptr<acceptor_type> acceptor_;
	};

	template <class ConnectionT>
	void boost_server<ConnectionT>::async_wait_for_connection()
	{
		acceptor_->async_accept(
			[this](const boost::system::error_code& error_code, asio_socket_type asio_socket)
			{
				if (error_code)
				{
					if (error_code != boost::asio::error::operation_aborted)
					{
						LOG_ERR(error_code.what());
					}

					return;
				}

				const auto local_endpoint = asio_socket.local_endpoint();

				const auto remote_endpoint = asio_socket.remote_endpoint();
				const auto endpoint_address = remote_endpoint.address();

				LOG_INFO("accepting connection from {} on port {}", endpoint_address.to_string(), local_endpoint.port());

				auto socket = std::make_unique<boost_tcp_socket>(executor_, std::move(asio_socket), ssl_context_);
				auto connection = std::make_shared<ConnectionT>(std::move(socket), this->shared_from_this());

				add_connection(std::move(connection));

				async_wait_for_connection();
			}
		);
	}

	template <class ConnectionT>
	void boost_server<ConnectionT>::stop()
	{
		boost::system::error_code ec;
		acceptor_->close(ec);

		server::stop();
	}
}
