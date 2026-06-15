#pragma once
#include "request_def.hpp"
#include <serialisation/serialisation.hpp>
#include <log/log.hpp>

#include <span>
#include <memory>

namespace sl
{
	class connection;
}

namespace sl::request
{
	template <class RequestType>
	struct request_info
	{
		using handler_type = void(*)(const std::shared_ptr<connection>&, const RequestType*);

		request_id_t id;
		handler_type handler;

		[[nodiscard]] bool process(
			const request_id_t actual_id,
			const std::shared_ptr<connection>& conn,
			std::span<const std::uint8_t> body) const
		{
			if (actual_id != id)
			{
				return false;
			}

			if (!serialisation::is_valid<RequestType>(body))
			{
				LOG_ERR("invalid request body for type {}", id);
				return true;
			}

			handler(conn, serialisation::deserialise<RequestType>(body));
			return true;
		}
	};

	template <auto&... Handlers>
	struct request_router
	{
		[[nodiscard]] static bool dispatch(
			const request_id_t id,
			const std::shared_ptr<connection>& conn,
			std::span<const std::uint8_t> body)
		{
			return (Handlers.process(id, conn, body) || ...);
		}
	};
}
