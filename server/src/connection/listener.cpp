#include "listener.hpp"

namespace sl
{
	void connection_listener::set_timeout(const timeout_duration timeout) noexcept
	{
		timeout_ = timeout;
	}

	void connection_listener::add_connection(std::shared_ptr<connection> connection)
	{
		if (timeout_ != timeout_duration::zero())
		{
			connection->socket().set_timeout(timeout_);
		}

		connection->async_handshake(sl::socket::handshake_type::server,
			[this, connection](const bool is_valid)
			{
				if (is_valid)
				{
					LOG_INFO("handshake was successful");

					{
						const std::lock_guard lock(connections_mutex_);
						connections_.push_back(connection);
					}

					connection->await_request();
				}
				else
				{
					LOG_ERR("failed to handshake");
				}
			}
		);
	}

	void connection_listener::remove_connection(connection* const connection)
	{
		const std::lock_guard lock(connections_mutex_);

		std::erase_if(connections_,
			[connection](const std::shared_ptr<sl::connection>& entry)
			{
				return entry.get() == connection;
			}
		);
	}
}
