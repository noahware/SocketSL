#include "client_connection.hpp"

#include <router/router.hpp>
#include <response/response.hpp>
#include <schema/schema.hpp>

#include <schema/request_generated.h>
#include <schema/response_generated.h>

#include <log/log.hpp>

namespace sl
{
	namespace
	{
		void handle_valid_test_request(const std::shared_ptr<connection>& connection, const Client::TestRequest* const request_body)
		{
			LOG_INFO("test request key: 0x{:X}", request_body->key());

			constexpr std::uint64_t response_key = 0x56789;

			response::send(connection->socket(),
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

		constexpr message_info<Client::TestRequest, connection> test_request{Client::RequestId_Test, handle_valid_test_request};

		using router = message_router<test_request>;
	}

	void client_connection::handle_request(const request::request_id_t request_id, const std::shared_ptr<std::vector<std::uint8_t>> body_buffer)
	{
		if (!router::dispatch(request_id, shared_as<connection>(), *body_buffer))
		{
			LOG_ERR("unknown request type: {}", request_id);
		}
	}
}
