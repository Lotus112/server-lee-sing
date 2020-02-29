#include <boost/asio.hpp> 
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include <fstream>
#include <atomic>
#include <thread>
#include <iostream>

using namespace boost;
using namespace std;



class Service {
    static const std::map<unsigned int,string> http_status_table;

    public: 
        Service(std::shared_ptr<boost::asio::ip::tcp::socket> sock) :
            m_sock(sock),
            m_request(4096),
            m_response_status_code(200),
            m_resource_size_bytes(0)
        {};
        void start_handling() {
            asio::async_read_until(*m_sock.get(), m_request,"\r\n",[this]( const boost::system::error_code& ec, size_t bytes_transferred)
            {
                on_request_line_received(ec, bytes_transferred);
            });
        }
    
    private: 
        void on_request_line_received(const boost::system::error_code& ec, size_t bytes_transferred)
        {
            if (ec) {
                cout << "Error occured! Error code = " << ec.value() << ". Message: " << ec.message();
                if (ec == asio::error::not_found) 
                {
                    // No delimiter has been found in the
                    // request message.
                    m_response_status_code = 413;
                    send_response();
                    return;
                }
                else {
                    // In case of any other error –
                    // close the socket and clean up.
                    on_finish();
                    return;
                }
            }

            // Parse the request line.
            string request_line;
            std::istream request_stream(&m_request);
            getline(request_stream, request_line, '\r');
            // Remove symbol '\n' from the buffer.
            request_stream.get();

            // Parse the request line.
            string request_method; 
            istringstream request_line_stream(request_line);
            request_line_stream >> request_method;

            // We only support GET method.
            if (request_method.compare("GET") != 0) {
                // Unsupported method.
                m_response_status_code = 501;
                send_response();
                return;
            }

            request_line_stream >> m_requested_resource;

            string request_http_version;
            request_line_stream >> request_http_version;

            if (request_http_version.compare("HTTP/1.1") != 0) {
                // Unsupported HTTP version or bad request.
                m_response_status_code = 505;
                send_response();
                return;
            }

            // At this point the request line is successfully
            // received and parsed. Now read the request headers.
            asio::async_read_until(*m_sock.get(),m_request,"\r\n\r\n", [this]( const boost::system::error_code& ec, size_t bytes_transferred)
            {
                on_headers_received(ec, bytes_transferred);
            });

            return;
        }

        void on_headers_received(const boost::system::error_code& ec,size_t bytes_transferred)
            {
            if (ec) {
                cout << "Error occured! Error code = " << ec.value() << ". Message: " << ec.message();
                if (ec == asio::error::not_found) {
                    // No delimiter has been found in the
                    // request message.
                    m_response_status_code = 413; 
                    send_response();
                    return;
                }
                else {
                    // In case of any other error - close the
                    // socket and clean up.
                    on_finish();
                    return;
                }
            }
            // Parse and store headers.
            std::istream request_stream(&m_request);
            string header_name, header_value;

            while (!request_stream.eof()) {
                std::getline(request_stream, header_name, ':');
                if (!request_stream.eof()) {
                    std::getline(request_stream, header_value, '\r');
                    // Remove symbol \n from the stream.
                    request_stream.get();
                    m_request_headers[header_name] = header_value;
                }
            }
            // Now we have all we need to process the request.
            process_request();
            send_response();
            return;
        }

        void process_request() {
            // Read file.
            string resource_file_path = string("D:\\http_root") + m_requested_resource;
            if (!boost::filesystem::exists(resource_file_path)) {
                // Resource not found.
                m_response_status_code = 404;
                return;
            }

            std::ifstream resource_fstream(resource_file_path,std::ifstream::binary);
            if (!resource_fstream.is_open()) {
                // Could not open file.
                // Something bad has happened.
                m_response_status_code = 500;
                return;
            }

            // Find out file size.
            resource_fstream.seekg(0, std::ifstream::end);
            m_resource_size_bytes = static_cast<size_t>(resource_fstream.tellg());
            m_resource_buffer.reset(new char[m_resource_size_bytes]);

            resource_fstream.seekg(std::ifstream::beg);
            resource_fstream.read(m_resource_buffer.get(),m_resource_size_bytes);
            m_response_headers += string("content-length") + ": " + to_string(m_resource_size_bytes) + "\r\n";
        }

        void send_response() 
        {
            m_sock->shutdown(asio::ip::tcp::socket::shutdown_receive);

            auto status_line = http_status_table.at(m_response_status_code);
            m_response_status_line = std::string("HTTP/1.1 ") + status_line + "\r\n";
            m_response_headers += "\r\n";
            std::vector<asio::const_buffer> response_buffers;
            response_buffers.push_back (asio::buffer(m_response_status_line));

            if (m_response_headers.length() > 0) {
                response_buffers.push_back(asio::buffer(m_response_headers));
            }
            if (m_resource_size_bytes > 0) {
                response_buffers.push_back(asio::buffer(m_resource_buffer.get(), m_resource_size_bytes));
            }
            
            // Initiate asynchronous write operation.
            asio::async_write(*m_sock.get(), response_buffers, [this] (
                const boost::system::error_code& ec,
                std::size_t bytes_transferred)
            {
                on_response_sent(ec, bytes_transferred);
            });
        }

        void on_response_sent(const boost::system::error_code& ec, std::size_t bytes_transferred) 
        {
            if (ec) {
                std::cout << * "Error occured Error code = " << ec.value () << ". Message : " << ec.message () ;
            }
            m_sock->shutdown(asio::ip::tcp::socket::shutdown_both);
            on_finish () ;   
        }

        void on_finish()
        {
            delete this;
        }
    
    private:
        std::shared_ptr<boost::asio::ip::tcp::socket> m_sock;
        boost::asio::streambuf m_request;
        std::map<string, string> m_request_headers;
        string m_requested_resource;

        std::unique_ptr<char[]> m_resource_buffer;
        unsigned int m_response_status_code;
        size_t m_resource_size_bytes; 
        string m_response_headers; 
        string m_response_status_line;
    };

    const std::map<unsigned int, string>
        Service::http_status_table =
    {
        { 200, "200 OK" },
        { 404, "404 Not Found" },
        { 413, "413 Request Entity Too Large" },
        { 500, "500 Server Error" },
        { 501, "501 Not Implemented" },
        { 505, "505 HTTP Version Not Supported" }
    };

class Acceptor {
    public:
        Acceptor(asio::io_context&ios, unsigned short port_num) :
        m_ios(ios),
        m_acceptor(m_ios, asio::ip::tcp::endpoint(asio::ip::address_v4::any(),port_num)), m_isStopped(false)
        {}

        void Start() 
        {
            m_acceptor.listen();
            InitAccept();
        }

        void Stop()
        {
            m_isStopped.store(true);
        }

    private:
        void InitAccept()
        {
            std::shared_ptr<asio::ip::tcp::socket> sock(new asio::ip::tcp::socket(m_ios));
            m_acceptor.async_accept(*sock.get(), [this, sock] (const boost::system::error_code& error)
            {
                onAccept(error, sock);
            });
        }

        void OnAccept(const boost::system::error_code&ec, std::shared_ptr<asio::ip::tcp::socket> sock)
        {
            if (ec)
            {
                
            }
        }
}     