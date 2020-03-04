#include "stream-client/stream-client.hpp"

#include <boost/algorithm/string/case_conv.hpp>
#include <iostream>
#include <regex>

template <typename T>
std::vector<std::thread> start_threads(const std::string& host, std::string& port,
                                       const boost::beast::http::request<boost::beast::http::string_body>& request,
                                       int threads_num, int req_per_thread)
{
    // by itself connector doesn't open a session, it only starts resolving thread to keep up with DNS changes
    // therefore, this operation is non-blocking call
    auto connector = std::make_shared<T>(host, port,
                                         std::chrono::milliseconds(5000), // resolve_timeout
                                         std::chrono::milliseconds(1000), // connect_timeout
                                         std::chrono::milliseconds(500), // operation_timeout
                                         stream_client::resolver::ip_family::ipv4);

    std::vector<std::thread> threads;
    for (int i = 0; i < threads_num; ++i) {
        threads.emplace_back([&request, connector, req_per_thread]() {
            // give each thread its' own session and reuse it to send-out some requests
            auto session = connector->new_session();
            for (int j = 0; j < req_per_thread; ++j) {
                const auto resp = session->perform(request);
                std::cout << resp;
            }
        });
    }
    return threads;
}

struct ParsedURI
{
    std::string protocol;
    std::string domain;
    std::string port;
    std::string resource;
    std::string query;

    ParsedURI(const std::string& url)
    {
        static const std::regex PARSE_URL{R"(((http|https)://)?([^/ :]+)(:(\d+))?(/([^ ?]+)?)?/?\??([^/ ]+\=[^/ ]+)?)",
                                          std::regex_constants::ECMAScript | std::regex_constants::icase};

        auto value_or = [](const std::string& value, std::string&& deflt) { return (value.empty() ? deflt : value); };
        // Note: only "http" and "https" protocols are supported
        std::smatch match;
        if (std::regex_match(url, match, PARSE_URL) && match.size() == 9) {
            domain = match[3];
            protocol = value_or(boost::algorithm::to_lower_copy(std::string(match[2])), "http");
            port = value_or(match[5], (protocol == "https") ? "443" : "80");
            resource = value_or(match[6], "/");
            query = match[8];
        }
    }
};

int main(int argc, char* argv[])
{
    if (argc != 4) {
        std::cerr << "Usage: client <url> <threads> <requests per thread>\n";
        return 1;
    }
    int num_threads = std::atoi(argv[2]);
    int requests = std::atoi(argv[3]);
    ParsedURI uri(argv[1]);

    boost::beast::http::request<boost::beast::http::string_body> req;
    req.version(11);
    req.method(boost::beast::http::verb::post);
    req.target(uri.resource);
    req.set(boost::beast::http::field::host, uri.domain);
    req.body() = "{test}";
    req.set(boost::beast::http::field::content_type, "application/json");
    req.set(boost::beast::http::field::accept, "*/*");
    req.set(boost::beast::http::field::user_agent, "beast/stream_client");
    req.prepare_payload();

    std::cout << std::boolalpha;
    std::cout << req;

    std::vector<std::thread> threads;
    if (uri.protocol == "http") {
        threads = start_threads<stream_client::connector::http_connector>(uri.domain, uri.port, req, num_threads,
                                                                          requests);
    } else if (uri.protocol == "https") {
        threads = start_threads<stream_client::connector::https_connector>(uri.domain, uri.port, req, num_threads,
                                                                           requests);
    } else {
        std::cerr << "protocol should be either 'http' or 'https'" << std::endl;
        return 1;
    }
    for (auto& t : threads) {
        t.join();
    }

    return 0;
}
