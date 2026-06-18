#pragma once
#include <network/socket.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <type_traits>
#include <vector>

namespace sl
{
	// async framed-message read loop over a socket.
	// wire frame: [uint64 frame size][header flatbuffer { type, body_size }][body]
	class session : public std::enable_shared_from_this<session>
	{
	public:
		using message_id_t = std::uint8_t;
		using frame_size_t = std::uint64_t;
		using body_buffer_t = std::shared_ptr<std::vector<std::uint8_t>>;

		explicit session(std::unique_ptr<sl::socket> socket) noexcept;
		virtual ~session();

		session(const session&) = delete;
		session& operator=(const session&) = delete;

		[[nodiscard]] sl::socket& socket() const noexcept;

		void start();
		void stop();

	protected:
		// parse a header buffer into (type, body_size); return false on protocol error
		[[nodiscard]] virtual bool parse_header(std::span<const std::uint8_t> header,
												message_id_t& out_type,
												std::size_t& out_body_size) const = 0;

		// a complete message body has been received
		virtual void on_message(message_id_t type, body_buffer_t body) = 0;

		// any read or protocol failure; default closes the socket
		virtual void on_error();

		template <class Self>
		[[nodiscard]] std::shared_ptr<Self> shared_as() noexcept
		{
			static_assert(std::is_base_of_v<session, Self>, "shared_as<Self>: Self must derive from session");

			return std::static_pointer_cast<Self>(shared_from_this());
		}

		std::unique_ptr<sl::socket> socket_;

	private:
		void read_frame_size();
		void read_header(std::size_t header_size);
		void read_body(message_id_t type, std::size_t body_size);
	};
}
