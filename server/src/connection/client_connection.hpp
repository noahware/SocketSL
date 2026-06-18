#pragma once
#include <connection/server_session.hpp>

#include <memory>
#include <vector>

namespace sl
{
	class client_connection final : public server_session
	{
	public:
		using server_session::server_session;

	protected:
		void handle_request(request::request_id_t request_id, std::shared_ptr<std::vector<std::uint8_t>> body_buffer) override;
	};
}
