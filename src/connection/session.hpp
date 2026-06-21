#pragma once
#include <network/socket.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

namespace sl
{
	class session_manager;

	// async framed-message session over a socket, used for both directions.
	// wire frame: [uint64 frame size][MessageHeader { type, body_size }][body]
	class session : public std::enable_shared_from_this<session>
	{
	public:
		using message_id_t = std::uint8_t;
		using frame_size_t = std::uint64_t;
		using body_buffer_t = std::shared_ptr<std::vector<std::uint8_t>>;

		explicit session(std::unique_ptr<sl::socket> socket, std::shared_ptr<session_manager> manager = nullptr) noexcept;
		virtual ~session();

		session(const session&) = delete;
		session& operator=(const session&) = delete;

		[[nodiscard]] sl::socket& socket() const noexcept;

		[[nodiscard]] bool connect(std::string_view host, std::string_view service) const;
		void async_connect(std::string_view host, std::string_view service, const async_callback_t& handler) const;
		[[nodiscard]] bool handshake(sl::socket::handshake_type type) const;
		void async_handshake(sl::socket::handshake_type type, const async_callback_t& handler) const;

		void start();
		void stop();

	protected:
		void handle_sys_message(message_id_t id, body_buffer_t body);

		// a complete message body has been received
		virtual void handle_message(message_id_t id, body_buffer_t body) = 0;

		// any read or protocol failure; default deregisters (if managed) or closes the socket
		virtual void on_error();

		template <class Self>
		[[nodiscard]] std::shared_ptr<Self> shared_as() noexcept
		{
			static_assert(std::is_base_of_v<session, Self>, "shared_as<Self>: Self must derive from session");

			return std::static_pointer_cast<Self>(shared_from_this());
		}

		std::unique_ptr<sl::socket> socket_;
		std::shared_ptr<session_manager> manager_;

	private:
		[[nodiscard]] static bool parse_header(std::span<const std::uint8_t> header, message_id_t& out_type, std::size_t& out_body_size, bool& out_is_system);

		void read_frame_size();
		void read_header(std::size_t header_size);
		void read_body(message_id_t type, std::size_t body_size, bool is_system);
	};
}
