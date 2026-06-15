#pragma once
#include <memory>
#include <network/socket.hpp>
#include <request/request_def.hpp>

namespace sl
{
	class connection_listener;

	class connection : public std::enable_shared_from_this<connection>
	{
	public:
		explicit connection(std::unique_ptr<sl::socket> socket, std::shared_ptr<connection_listener> parent_listener) noexcept
				:	socket_(std::move(socket)),
					parent_listener_(std::move(parent_listener)) {}

		~connection();

		[[nodiscard]] bool handshake(sl::socket::handshake_type type) const;
		void async_handshake(sl::socket::handshake_type type, const async_callback_t& handler) const;

		void await_request();

		[[nodiscard]] sl::socket& socket() const noexcept;

	protected:
		virtual void handle_request(request::request_id_t request_id, std::shared_ptr<std::vector<std::uint8_t>> body_buffer) = 0;

		void close_self();

		void read_request_header_size();
		void read_request_header(std::size_t header_size);
		void read_request_body(request::request_id_t request_id, std::size_t body_size);

		std::unique_ptr<sl::socket> socket_;
		std::shared_ptr<connection_listener> parent_listener_;
	};

	class client_connection final : public connection
	{
	public:
		explicit client_connection(std::unique_ptr<sl::socket> socket, std::shared_ptr<connection_listener> parent_listener) noexcept
				:	connection(std::move(socket), std::move(parent_listener)) {}

	protected:
		void handle_request(request::request_id_t request_id, std::shared_ptr<std::vector<std::uint8_t>> body_buffer) override;
	};
}
