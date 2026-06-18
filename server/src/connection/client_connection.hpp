#pragma once
#include <connection/connection.hpp>

#include <memory>
#include <vector>

namespace sl
{
	class client_connection final : public connection
	{
	public:
		using connection::connection;

	protected:
		void handle_request(request::request_id_t request_id, std::shared_ptr<std::vector<std::uint8_t>> body_buffer) override;
	};
}
