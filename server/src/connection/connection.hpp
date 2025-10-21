#pragma once
#include <memory>
#include <network/socket.hpp>
#include <request/request_def.hpp>

class connection_listener_t;

class connection_t : public std::enable_shared_from_this<connection_t>
{
public:
	explicit connection_t(std::unique_ptr<socket_t> socket, std::shared_ptr<connection_listener_t> parent_listener)
			:	socket_(std::move(socket)),
				parent_listener_(std::move(parent_listener)) {}

	~connection_t();

	[[nodiscard]] std::uint8_t handshake(socket_t::handshake_type_t type) const;
	void async_handshake(socket_t::handshake_type_t type, const async_callback_t& handler) const;

	void await_request();

	socket_t& socket() const;

protected:
	virtual void handle_request(request::request_id_t request_id, std::shared_ptr<std::vector<std::uint8_t>> body_buffer) = 0;

	void close_self();

	void read_request_header_size();
	void read_request_header(request::request_buffer_size_t header_size);
	void read_request_body(request::request_id_t request_id, request::request_buffer_size_t body_size);

	std::unique_ptr<socket_t> socket_;
	std::shared_ptr<connection_listener_t> parent_listener_;
};

class client_connection_t final : public connection_t
{
public:
	explicit client_connection_t(std::unique_ptr<socket_t> socket, std::shared_ptr<connection_listener_t> parent_listener)
			:	connection_t(std::move(socket), std::move(parent_listener)) {}

protected:
	void handle_request(request::request_id_t request_id, std::shared_ptr<std::vector<std::uint8_t>> body_buffer) override;
};
