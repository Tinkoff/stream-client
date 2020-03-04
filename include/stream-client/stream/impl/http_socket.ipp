#pragma once

namespace stream_client {
namespace http {

template <typename Stream>
template <typename Body, typename Fields>
boost::optional<boost::beast::http::response<Body, Fields>>
base_socket<Stream>::perform(const boost::beast::http::request<Body, Fields>& request, boost::system::error_code& ec,
                             const time_point_type& deadline)
{
    send_request(request, ec, deadline);
    if (ec) {
        return boost::none;
    }
    return recv_response<Body, Fields>(ec, deadline);
}

template <typename Stream>
template <typename Body, typename Fields>
void base_socket<Stream>::send_request(const boost::beast::http::request<Body, Fields>& request,
                                       boost::system::error_code& ec, const time_point_type& deadline)
{
    boost::beast::http::request_serializer<Body, Fields> serializer(request);
    serializer.split(false);

    std::size_t n_bytes = 0;
    while (!serializer.is_done()) {
        n_bytes = 0;
        serializer.next(ec, [this, &n_bytes, &deadline](boost::system::error_code& ec, const auto& buffers) {
            n_bytes = stream_.write_some(buffers, ec, deadline);
        });
        if (n_bytes) {
            serializer.consume(n_bytes);
        }
        if (ec) {
            break;
        }
    }
}

template <typename Stream>
template <typename Parser, typename DynamicBuffer>
void base_socket<Stream>::recv_response(Parser& response_parser, DynamicBuffer& buffer, boost::system::error_code& ec,
                                        const time_point_type& deadline)
{
    std::size_t n_bytes = 0;
    while (!response_parser.is_done()) {
        // obtain writable buffer sequence
        boost::optional<typename DynamicBuffer::mutable_buffers_type> out_buff;
        const auto read_size = std::min<std::size_t>(65536, buffer.max_size() - buffer.size() - 1);
        try {
            out_buff.emplace(buffer.prepare(read_size));
        } catch (const std::length_error&) {
            ec = boost::beast::http::error::buffer_overflow;
            break;
        }
        // read data from stream into writable buffer
        n_bytes = stream_.read_some(*out_buff, ec, deadline);
        if (ec == boost::asio::error::eof) {
            if (response_parser.got_some()) {
                // caller sees EOF on next read
                response_parser.put_eof(ec);
                if (!ec) {
                    continue;
                }
            } else {
                ec = boost::beast::http::error::end_of_stream;
            }
        }
        if (ec) {
            break;
        }
        // commit written data from writable buffer to readable one
        buffer.commit(n_bytes);

        // parse data from redable buffer
        n_bytes = response_parser.put(buffer.data(), ec);
        buffer.consume(n_bytes);
        if (ec && ec != boost::beast::http::error::need_more) {
            break;
        }
    }
}

template <typename Stream>
template <typename Body, typename Fields>
boost::optional<boost::beast::http::response<Body, Fields>>
base_socket<Stream>::recv_response(boost::system::error_code& ec, const time_point_type& deadline)
{
    boost::beast::http::response_parser<Body, typename Fields::allocator_type> parser;
    parser.header_limit(kHeaderLimit);
    parser.body_limit(kBodyLimit);
    parser.eager(true);

    buffer_.consume(buffer_.size());
    recv_response(parser, buffer_, ec, deadline);
    if (ec) {
        return boost::none;
    }
    return parser.get();
}

} // namespace http
} // namespace stream_client
