#pragma once
#include <boost/asio/ssl.hpp>

class ssl_context_t
{
public:
	enum class crypto_file_format_t : std::uint8_t
	{
		asn1,
		pem
	};

	ssl_context_t() = default;
	virtual ~ssl_context_t() = default;

	virtual void disable_peer_verification() = 0;
	virtual void require_peer_verification() = 0;

	virtual void use_certificate(const std::string& path_to_certificate, crypto_file_format_t file_format) = 0;
	virtual void use_private_key(const std::string& path_to_key, crypto_file_format_t file_format) = 0;

	virtual void disable_certificate_verification() = 0;
};

class boost_ssl_context_t final : public ssl_context_t
{
public:
	typedef boost::asio::ssl::context asio_ssl_t;
	typedef boost::asio::ssl::context::method ssl_method_t;
	typedef boost::asio::ssl::context::options ssl_options_t;
	typedef boost::asio::ssl::context::file_format ssl_file_format_t;

	boost_ssl_context_t() = delete;
	
	explicit boost_ssl_context_t(const ssl_method_t ssl_method)
		: native_handle_(std::make_unique<asio_ssl_t>(ssl_method)) {}

	void disable_peer_verification() override;
	void require_peer_verification() override;

	void use_certificate(const std::string& path_to_certificate, crypto_file_format_t file_format) override;
	void use_private_key(const std::string& path_to_key, crypto_file_format_t file_format) override;

	void disable_certificate_verification() override;

	void set_options(ssl_options_t options);
	void clear_options(ssl_options_t options);

	asio_ssl_t& native_handle() const;

protected:
	std::unique_ptr<asio_ssl_t> native_handle_;
};
