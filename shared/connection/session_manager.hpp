#pragma once
#include "session.hpp"

#include <log/log.hpp>

#include <mutex>

namespace sl
{
	class session_manager
	{
	public:
		using timeout_duration = socket::timeout_duration;

		session_manager() = default;
		virtual ~session_manager() = default;

		void set_timeout(timeout_duration timeout) noexcept;

		virtual void async_wait_for_connection() = 0;
		virtual void stop();

		void add_session(std::shared_ptr<session> sess);
		void connect(std::shared_ptr<session> sess, std::string_view host, std::string_view service);
		void remove_session(session* sess);

		template <class Fn>
		void for_each_session(Fn&& fn) const
		{
			const std::lock_guard lock(sessions_mutex_);

			for (const auto& sess : sessions_)
			{
				fn(sess);
			}
		}

		[[nodiscard]] std::size_t session_count() const;

	protected:
		void register_and_start(std::shared_ptr<session> sess);

		timeout_duration timeout_{};
		mutable std::mutex sessions_mutex_;
		std::vector<std::shared_ptr<session>> sessions_;
	};

	// must be created as a shared ptr
	template <class SessionT>
	class boost_session_manager final : public session_manager, public std::enable_shared_from_this<boost_session_manager<SessionT>>
	{
		static_assert(std::is_base_of_v<session, SessionT>, "SessionT must derive from session");

	public:
		using executor_type = boost::asio::any_io_executor;
		using tcp_type = boost::asio::ip::tcp;
		using acceptor_type = boost::asio::ip::tcp::acceptor;
		using endpoint_type = boost::asio::ip::tcp::endpoint;
		using asio_socket_type = boost::asio::ip::tcp::socket;

		boost_session_manager(executor_type executor, std::shared_ptr<boost_ssl_context> ssl_context, const std::uint16_t port)
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

	template <class SessionT>
	void boost_session_manager<SessionT>::async_wait_for_connection()
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
				auto sess = std::make_shared<SessionT>(std::move(socket), this->shared_from_this());

				add_session(std::move(sess));

				async_wait_for_connection();
			}
		);
	}

	template <class SessionT>
	void boost_session_manager<SessionT>::stop()
	{
		boost::system::error_code ec;
		acceptor_->close(ec);

		session_manager::stop();
	}
}
