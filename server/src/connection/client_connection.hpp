#pragma once
#include <connection/session.hpp>

#include <memory>
#include <vector>

namespace sl
{
	class client_connection final : public session
	{
	public:
		using session::session;

	protected:
		void handle_message(message_id_t id, body_buffer_t body) override;
	};
}
