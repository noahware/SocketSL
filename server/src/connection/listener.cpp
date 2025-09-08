#include "listener.hpp"

void connection_listener_t::add_connection(std::shared_ptr<connection_t> connection)
{
	connection->async_handshake(socket_t::handshake_type_t::server,
		[this, connection](const std::uint8_t is_valid)
		{
			if (is_valid)
			{
				spdlog::info("handshake was successful");

				connections_.push_back(connection);

				connection->await_request();
			}
			else
			{
				spdlog::error("failed to handshake");
			}
		}
	);
}

void connection_listener_t::remove_connection(connection_t* const connection)
{
	std::erase_if(connections_,
		[connection](const std::shared_ptr<connection_t>& entry)
		{
			return entry.get() == connection;
		}
	);
}
