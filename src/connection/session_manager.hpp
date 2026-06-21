#pragma once
#include "session.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace sl
{
	struct reconnect_policy
	{
		std::chrono::steady_clock::duration initial_delay{std::chrono::seconds(1)};
		std::chrono::steady_clock::duration max_delay{std::chrono::seconds(30)};
		std::chrono::steady_clock::duration idle_timeout{std::chrono::seconds(30)};
		double multiplier = 2.0;
		std::size_t max_attempts = 0;
	};

	class session_manager
	{
	public:
		using timeout_duration = socket::timeout_duration;
		using session_callback_t = std::function<void(const std::shared_ptr<session>&)>;

		session_manager() = default;
		virtual ~session_manager() = default;

		void set_idle_timeout(timeout_duration timeout) noexcept;
		void set_heartbeat_timeout(timeout_duration timeout) noexcept;
		void set_handshake_timeout(timeout_duration timeout) noexcept;
		void set_max_message_size(std::size_t max_size) noexcept;
		void set_max_pending_writes(std::size_t max_pending) noexcept;

		// 0 = unlimited. inbound connections past either limit are rejected before the handshake
		void set_max_sessions(std::size_t max) noexcept;
		void set_max_connections_per_ip(std::size_t max) noexcept;

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
		void apply_caps(session& sess) const;
		virtual void on_session_removed(const std::shared_ptr<session>&) {}

		void register_and_start(std::shared_ptr<session> sess);

		// admission control for an inbound connection from ip: false if a cap would be exceeded
		[[nodiscard]] bool can_ip_connect(std::uint32_t ip) const;

		timeout_duration idle_timeout_{};
		timeout_duration heartbeat_timeout_{};
		timeout_duration handshake_timeout_{};
		std::size_t max_message_size_{};
		std::size_t max_pending_writes_{};
		std::size_t max_sessions_{};
		std::size_t max_connections_per_ip_{};
		session_callback_t on_connect_;
		session_callback_t on_disconnect_;
		mutable std::mutex sessions_mutex_;
		std::vector<std::shared_ptr<session>> sessions_;
		std::unordered_map<std::uint32_t, std::size_t> connections_per_ip_;
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

		// like connect(), but re-dials automatically with exponential backoff when the session drops
		std::shared_ptr<SessionT> connect(std::string_view host, std::string_view service,
			std::shared_ptr<boost_ssl_context> ssl_context, const reconnect_policy& policy);

	private:
		struct reconnect_state
		{
			std::string host;
			std::string service;
			std::shared_ptr<boost_ssl_context> ssl_context;
			reconnect_policy policy;
			std::size_t attempts = 0;
			timeout_duration current_delay{};
		};

		void on_session_removed(const std::shared_ptr<session>& sess) override;
		void do_connect(std::shared_ptr<SessionT> sess, std::string host, std::string service,
			std::optional<reconnect_state> state = std::nullopt);
		void schedule_reconnect(reconnect_state state);
		void attempt_reconnect(reconnect_state state);

		std::mutex reconnect_mutex_;
		std::unordered_map<session*, reconnect_state> reconnect_map_;
		std::atomic<bool> reconnect_stopped_{false};

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
		reconnect_stopped_ = true;

		{
			const std::lock_guard lock(reconnect_mutex_);
			reconnect_map_.clear();
		}

		boost::system::error_code ec;
		acceptor_->close(ec);

		session_manager::stop();
	}

	template <class SessionT>
	std::shared_ptr<SessionT> boost_session_manager<SessionT>::connect(const std::string_view host, const std::string_view service, std::shared_ptr<boost_ssl_context> ssl_context)
	{
		auto socket = std::make_unique<boost_tcp_socket>(executor_, std::move(ssl_context));
		auto sess = std::make_shared<SessionT>(std::move(socket), this->shared_from_this());

		do_connect(sess, std::string(host), std::string(service));

		return sess;
	}

	template <class SessionT>
	std::shared_ptr<SessionT> boost_session_manager<SessionT>::connect(
		const std::string_view host, const std::string_view service,
		std::shared_ptr<boost_ssl_context> ssl_context, const reconnect_policy& policy)
	{
		auto socket = std::make_unique<boost_tcp_socket>(executor_, ssl_context);
		auto sess = std::make_shared<SessionT>(std::move(socket), this->shared_from_this());

		reconnect_state state;
		state.host = std::string(host);
		state.service = std::string(service);
		state.ssl_context = std::move(ssl_context);
		state.policy = policy;
		state.current_delay = policy.initial_delay;

		do_connect(sess, std::string(host), std::string(service), std::move(state));

		return sess;
	}

	template <class SessionT>
	void boost_session_manager<SessionT>::on_session_removed(const std::shared_ptr<session>& sess)
	{
		reconnect_state state;
		bool found = false;

		{
			const std::lock_guard lock(reconnect_mutex_);

			const auto entry = reconnect_map_.find(sess.get());

			if (entry != reconnect_map_.end())
			{
				state = std::move(entry->second);
				reconnect_map_.erase(entry);
				found = true;
			}
		}

		if (found)
		{
			schedule_reconnect(std::move(state));
		}
	}

	template <class SessionT>
	void boost_session_manager<SessionT>::do_connect(std::shared_ptr<SessionT> sess,
		std::string host, std::string service, std::optional<reconnect_state> state)
	{
		apply_caps(*sess);

		if (state && state->policy.idle_timeout > timeout_duration::zero())
		{
			sess->socket().set_idle_timeout(state->policy.idle_timeout);
		}

		sess->async_connect(host, service,
			[this, sess, state = std::move(state)](const bool connected) mutable
			{
				if (state && reconnect_stopped_)
				{
					return;
				}

				if (!connected)
				{
					if (state)
					{
						schedule_reconnect(std::move(*state));
					}

					return;
				}

				sess->async_handshake(sl::socket::handshake_type::client,
					[this, sess, state = std::move(state)](const bool is_valid) mutable
					{
						if (state && reconnect_stopped_)
						{
							return;
						}

						if (!is_valid)
						{
							if (state)
							{
								schedule_reconnect(std::move(*state));
							}

							return;
						}

						if (state)
						{
							state->attempts = 0;
							state->current_delay = state->policy.initial_delay;

							{
								const std::lock_guard lock(reconnect_mutex_);
								reconnect_map_[sess.get()] = std::move(*state);
							}
						}

						register_and_start(sess);
					}
				);
			}
		);
	}

	template <class SessionT>
	void boost_session_manager<SessionT>::schedule_reconnect(reconnect_state state)
	{
		if (reconnect_stopped_)
		{
			return;
		}

		state.attempts++;

		if (state.policy.max_attempts != 0 && state.attempts > state.policy.max_attempts)
		{
			return;
		}

		auto timer = std::make_shared<boost::asio::steady_timer>(executor_);
		timer->expires_after(state.current_delay);
		timer->async_wait(
			[this, timer, state = std::move(state)](const boost::system::error_code& ec) mutable
			{
				if (ec || reconnect_stopped_)
				{
					return;
				}

				state.current_delay = std::min(
					std::chrono::duration_cast<timeout_duration>(state.current_delay * state.policy.multiplier),
					state.policy.max_delay);

				attempt_reconnect(std::move(state));
			}
		);
	}

	template <class SessionT>
	void boost_session_manager<SessionT>::attempt_reconnect(reconnect_state state)
	{
		if (reconnect_stopped_)
		{
			return;
		}

		auto socket = std::make_unique<boost_tcp_socket>(executor_, state.ssl_context);
		auto sess = std::make_shared<SessionT>(std::move(socket), this->shared_from_this());

		auto host = state.host;
		auto service = state.service;
		do_connect(sess, std::move(host), std::move(service), std::move(state));
	}
}
