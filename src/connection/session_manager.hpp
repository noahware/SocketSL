#pragma once
#include "session.hpp"

#include <functional>
#include <mutex>

namespace sl
{
	class session_manager
	{
	public:
		using timeout_duration = socket::timeout_duration;
		using session_callback_t = std::function<void(const std::shared_ptr<session>&)>;

		session_manager() = default;
		virtual ~session_manager() = default;

		void set_idle_timeout(timeout_duration timeout) noexcept;
		void set_heartbeat_timeout(timeout_duration timeout) noexcept;
		void set_max_message_size(std::size_t max_size) noexcept;
		void set_max_pending_writes(std::size_t max_pending) noexcept;

		void on_connect(session_callback_t callback);
		void on_disconnect(session_callback_t callback);

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

		timeout_duration idle_timeout_{};
		timeout_duration heartbeat_timeout_{};
		std::size_t max_message_size_{};
		std::size_t max_pending_writes_{};
		session_callback_t on_connect_;
		session_callback_t on_disconnect_;
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

		using session_manager::connect;

		// convenience: builds the socket + session, dials, and returns the new session
		std::shared_ptr<SessionT> connect(std::string_view host, std::string_view service, std::shared_ptr<boost_ssl_context> ssl_context);

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
					return;
				}

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

	template <class SessionT>
	std::shared_ptr<SessionT> boost_session_manager<SessionT>::connect(const std::string_view host, const std::string_view service, std::shared_ptr<boost_ssl_context> ssl_context)
	{
		auto socket = std::make_unique<boost_tcp_socket>(executor_, std::move(ssl_context));
		auto sess = std::make_shared<SessionT>(std::move(socket), this->shared_from_this());

		session_manager::connect(sess, host, service);

		return sess;
	}
}
