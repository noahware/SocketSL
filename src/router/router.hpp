#pragma once
#include <serialisation/serialisation.hpp>

#include <cstdint>
#include <span>
#include <memory>

namespace sl
{
	template <class BodyType, class SessionType>
	struct message_info
	{
		using handler_type = void(*)(const std::shared_ptr<SessionType>&, const BodyType*);

		std::uint8_t id;
		handler_type handler;

		[[nodiscard]] bool process(
			const std::uint8_t actual_id,
			const std::shared_ptr<SessionType>& session,
			std::span<const std::uint8_t> body) const
		{
			if (actual_id != id)
			{
				return false;
			}

			if (!serialisation::is_valid<BodyType>(body))
			{
				return true;
			}

			handler(session, serialisation::deserialise<BodyType>(body));
			return true;
		}
	};

	template <auto&... Handlers>
	struct message_router
	{
		template <class SessionType>
		[[nodiscard]] static bool dispatch(
			const std::uint8_t id,
			const std::shared_ptr<SessionType>& session,
			std::span<const std::uint8_t> body)
		{
			return (Handlers.process(id, session, body) || ...);
		}
	};
}
