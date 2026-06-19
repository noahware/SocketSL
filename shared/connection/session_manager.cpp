#include "session_manager.hpp"

namespace sl
{
	void session_manager::set_timeout(const timeout_duration timeout) noexcept
	{
		timeout_ = timeout;
	}

	void session_manager::register_and_start(std::shared_ptr<session> sess)
	{
		{
			const std::lock_guard lock(sessions_mutex_);
			sessions_.push_back(sess);
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
					LOG_INFO("handshake was successful");

					register_and_start(sess);
				}
				else
				{
					LOG_ERR("failed to handshake");
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
					LOG_ERR("failed to connect to peer");

					return;
				}

				sess->async_handshake(sl::socket::handshake_type::client,
					[this, sess](const bool is_valid)
					{
						if (is_valid)
						{
							LOG_INFO("outbound handshake was successful");

							register_and_start(sess);
						}
						else
						{
							LOG_ERR("failed to handshake with peer");
						}
					}
				);
			}
		);
	}

	void session_manager::remove_session(session* const sess)
	{
		const std::lock_guard lock(sessions_mutex_);

		std::erase_if(sessions_,
			[sess](const std::shared_ptr<sl::session>& entry)
			{
				return entry.get() == sess;
			}
		);
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
