#include "client_connection.hpp"

#include <router/router.hpp>
#include <message/message.hpp>
#include <schema/schema.hpp>

#include <schema/request_generated.h>
#include <schema/response_generated.h>

#include <log/log.hpp>

namespace sl
{
	namespace
	{
		void handle_valid_test_request(const std::shared_ptr<session>& sess, const Client::TestRequest* const request_body)
		{
			LOG_INFO("test request key: 0x{:X}", request_body->key());

			constexpr std::uint64_t response_key = 0x56789;

			msg::async_send(sess->socket(),
				Client::ResponseId_Test,
				[](const bool is_valid)
				{
					if (is_valid)
					{
						LOG_INFO("successfully sent response");
					}
					else
					{
						LOG_ERR("failed to send response");
					}
				},
				CREATION_WRAPPER(Client::CreateTestResponse), response_key);
		}

		constexpr message_info<Client::TestRequest, session> test_request{Client::RequestId_Test, handle_valid_test_request};

		using router = message_router<test_request>;
	}

	void client_connection::handle_message(const message_id_t id, const body_buffer_t body)
	{
		if (!router::dispatch(id, shared_as<session>(), *body))
		{
			LOG_ERR("unknown request type: {}", id);
		}
	}
}
