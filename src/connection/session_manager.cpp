#include "session_manager.hpp"

#include <algorithm>

namespace sl
{
	void session_manager::set_timeout(const timeout_duration timeout) noexcept
	{
		timeout_ = timeout;
	}

	void session_manager::on_connect(session_callback_t callback)
	{
		on_connect_ = std::move(callback);
	}

	void session_manager::on_disconnect(session_callback_t callback)
	{
		on_disconnect_ = std::move(callback);
	}

	void session_manager::register_and_start(std::shared_ptr<session> sess)
	{
		{
			const std::lock_guard lock(sessions_mutex_);
			sessions_.push_back(sess);
		}

		if (on_connect_)
		{
			on_connect_(sess);
		}

		sess->start();
	}

	void session_manager::add_session(std::shared_ptr<session> sess)
	{
		if (timeout_ != timeout_duration::zero())
		{
			sess->socket().set_timeout(timeout_);
		}

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
		if (timeout_ != timeout_duration::zero())
		{
			sess->socket().set_timeout(timeout_);
		}

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
		}

		if (on_disconnect_)
		{
			on_disconnect_(removed);
		}
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
	}
}
