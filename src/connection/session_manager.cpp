#include "session_manager.hpp"

#include <algorithm>

namespace sl
{
	void session_manager::set_idle_timeout(const timeout_duration timeout) noexcept
	{
		idle_timeout_ = timeout;
	}

	void session_manager::set_heartbeat_timeout(const timeout_duration timeout) noexcept
	{
		heartbeat_timeout_ = timeout;
	}

	void session_manager::set_handshake_timeout(const timeout_duration timeout) noexcept
	{
		handshake_timeout_ = timeout;
	}

	void session_manager::set_max_sessions(const std::size_t max) noexcept
	{
		max_sessions_ = max;
	}

	void session_manager::set_max_connections_per_ip(const std::size_t max) noexcept
	{
		max_connections_per_ip_ = max;
	}

	void session_manager::set_max_message_size(const std::size_t max_size) noexcept
	{
		max_message_size_ = max_size;
	}

	void session_manager::set_max_pending_writes(const std::size_t max_pending) noexcept
	{
		max_pending_writes_ = max_pending;
	}

	void session_manager::on_connect(session_callback_t callback)
	{
		on_connect_ = std::move(callback);
	}

	void session_manager::on_disconnect(session_callback_t callback)
	{
		on_disconnect_ = std::move(callback);
	}

	void session_manager::apply_caps(session& sess) const
	{
		if (idle_timeout_ != timeout_duration::zero())
		{
			sess.socket().set_idle_timeout(idle_timeout_);
		}

		if (heartbeat_timeout_ != timeout_duration::zero())
		{
			sess.socket().set_heartbeat_timeout(heartbeat_timeout_);
		}

		if (handshake_timeout_ != timeout_duration::zero())
		{
			sess.socket().set_handshake_timeout(handshake_timeout_);
		}

		if (max_message_size_ != 0)
		{
			sess.socket().set_max_message_size(max_message_size_);
		}

		if (max_pending_writes_ != 0)
		{
			sess.socket().set_max_pending_writes(max_pending_writes_);
		}
	}

	void session_manager::register_and_start(std::shared_ptr<session> sess)
	{
		{
			const std::lock_guard lock(sessions_mutex_);
			sessions_.push_back(sess);
			++connections_per_ip_[sess->socket().ipv4_address()];
		}

		if (on_connect_)
		{
			on_connect_(sess);
		}

		sess->start();
	}

	bool session_manager::can_ip_connect(const std::uint32_t ip) const
	{
		const std::lock_guard lock(sessions_mutex_);

		if (max_sessions_ != 0 && sessions_.size() >= max_sessions_)
		{
			return false;
		}

		if (max_connections_per_ip_ != 0)
		{
			const auto entry = connections_per_ip_.find(ip);

			if (entry != connections_per_ip_.end() && entry->second >= max_connections_per_ip_)
			{
				return false;
			}
		}

		return true;
	}

	void session_manager::add_session(std::shared_ptr<session> sess)
	{
		if (!can_ip_connect(sess->socket().ipv4_address()))
		{
			sess->socket().close();

			return;
		}

		apply_caps(*sess);

		sess->async_handshake(sl::socket::handshake_type::server,
			[this, sess](const bool is_valid)
			{
				if (is_valid)
				{
					register_and_start(sess);
				}
			}
		);
	}

	void session_manager::connect(std::shared_ptr<session> sess, const std::string_view host, const std::string_view service)
	{
		apply_caps(*sess);

		sess->async_connect(host, service,
			[this, sess](const bool connected)
			{
				if (!connected)
				{
					return;
				}

				sess->async_handshake(sl::socket::handshake_type::client,
					[this, sess](const bool is_valid)
					{
						if (is_valid)
						{
							register_and_start(sess);
						}
					}
				);
			}
		);
	}

	void session_manager::remove_session(session* const sess)
	{
		std::shared_ptr<session> removed;

		{
			const std::lock_guard lock(sessions_mutex_);

			const auto entry = std::find_if(sessions_.begin(), sessions_.end(),
				[sess](const std::shared_ptr<sl::session>& candidate)
				{
					return candidate.get() == sess;
				}
			);

			if (entry == sessions_.end())
			{
				return;
			}

			removed = std::move(*entry);
			sessions_.erase(entry);

			const auto ip_entry = connections_per_ip_.find(removed->socket().ipv4_address());

			if (ip_entry != connections_per_ip_.end() && --ip_entry->second == 0)
			{
				connections_per_ip_.erase(ip_entry);
			}
		}

		if (on_disconnect_)
		{
			on_disconnect_(removed);
		}

		on_session_removed(removed);
	}

	std::size_t session_manager::session_count() const
	{
		const std::lock_guard lock(sessions_mutex_);

		return sessions_.size();
	}

	void session_manager::stop()
	{
		const std::lock_guard lock(sessions_mutex_);

		for (const auto& sess : sessions_)
		{
			sess->socket().close();
		}

		sessions_.clear();
		connections_per_ip_.clear();
	}
}
