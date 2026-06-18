#pragma once
#include <network/socket.hpp>
#include <response/response.hpp>

#include <memory>

namespace sl
{
	// must be created as a shared_ptr (uses shared_from_this in async chain)
	class client_session : public std::enable_shared_from_this<client_session>
	{
	public:
		explicit client_session(std::unique_ptr<sl::socket> socket) noexcept;
		virtual ~client_session();

		[[nodiscard]] bool connect(std::string_view host, std::string_view service);
		[[nodiscard]] bool handshake();

		void start();
		void stop();

		[[nodiscard]] sl::socket& socket() noexcept;

	protected:
		virtual void handle_response(response::response_id_t id,
									  std::shared_ptr<std::vector<std::uint8_t>> body) = 0;

	private:
		void read_response_header_size();
		void read_response_header(std::size_t header_size);
		void read_response_body(response::response_id_t id, std::size_t body_size);

		std::unique_ptr<sl::socket> socket_;
	};
}
