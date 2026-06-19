#include "ssl.hpp"

namespace sl
{
	void boost_ssl_context::disable_peer_verification()
	{
		native_handle_->set_verify_mode(boost::asio::ssl::verify_none);
	}

	void boost_ssl_context::require_peer_verification()
	{
		native_handle_->set_verify_mode(boost::asio::ssl::verify_peer | boost::asio::ssl::verify_fail_if_no_peer_cert);
	}

	void boost_ssl_context::load_verify_file(const std::string& path_to_file)
	{
		native_handle_->load_verify_file(path_to_file);
	}

	void boost_ssl_context::add_certificate_authority(const std::span<const std::uint8_t> buffer)
	{
		native_handle_->add_certificate_authority(boost::asio::buffer(buffer));
	}

	void boost_ssl_context::use_tmp_dh_file(const std::string& path_to_file)
	{
		native_handle_->use_tmp_dh_file(path_to_file);
	}

	void boost_ssl_context::use_tmp_dh(const std::span<const std::uint8_t> buffer)
	{
		native_handle_->use_tmp_dh(boost::asio::buffer(buffer));
	}

	void boost_ssl_context::use_certificate(const std::string& path_to_certificate, const crypto_file_format file_format)
	{
		const ssl_file_format_type ssl_ff = ssl_file_format(file_format);

		native_handle_->use_certificate_file(path_to_certificate, ssl_ff);
	}

	void boost_ssl_context::use_certificate(const std::span<const std::uint8_t> buffer, const crypto_file_format file_format)
	{
		const ssl_file_format_type ssl_ff = ssl_file_format(file_format);

		native_handle_->use_certificate(boost::asio::buffer(buffer), ssl_ff);
	}

	void boost_ssl_context::use_private_key(const std::string& path_to_key, const crypto_file_format file_format)
	{
		const ssl_file_format_type ssl_ff = ssl_file_format(file_format);

		native_handle_->use_private_key_file(path_to_key, ssl_ff);
	}

	void boost_ssl_context::use_private_key(const std::span<const std::uint8_t> buffer, const crypto_file_format file_format)
	{
		const ssl_file_format_type ssl_ff = ssl_file_format(file_format);

		native_handle_->use_private_key(boost::asio::buffer(buffer), ssl_ff);
	}

	void boost_ssl_context::set_options(const ssl_options_type options)
	{
		native_handle_->set_options(options);
	}

	void boost_ssl_context::clear_options(const ssl_options_type options)
	{
		native_handle_->clear_options(options);
	}

	boost_ssl_context::asio_ssl_type& boost_ssl_context::native_handle() const noexcept
	{
		return *native_handle_;
	}

	boost_ssl_context::ssl_file_format_type boost_ssl_context::ssl_file_format(const crypto_file_format file_format) noexcept
	{
		return file_format == crypto_file_format::asn1 ? ssl_file_format_type::asn1 : ssl_file_format_type::pem;
	}
}
