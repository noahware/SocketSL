#include "listener.hpp"

namespace sl
{
	void server::set_timeout(const timeout_duration timeout) noexcept
	{
		timeout_ = timeout;
	}

	void server::add_connection(std::shared_ptr<server_session> connection)
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

					connection->start();
				}
				else
				{
					LOG_ERR("failed to handshake");
				}
			}
		);
	}

	void server::remove_connection(server_session* const connection)
	{
		const std::lock_guard lock(connections_mutex_);

		std::erase_if(connections_,
			[connection](const std::shared_ptr<sl::server_session>& entry)
			{
				return entry.get() == connection;
			}
		);
	}

	std::size_t server::conn_count() const noexcept
	{
		const std::lock_guard lock(connections_mutex_);

		return connections_.size();
	}

	void server::stop()
	{
		const std::lock_guard lock(connections_mutex_);

		for (const auto& conn : connections_)
		{
			conn->socket().close();
		}

		connections_.clear();
	}
}
