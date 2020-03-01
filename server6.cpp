#include "server6.hpp"

void Service::on_request_line_received(const boost::system::error_code& ec, size_t bytes_transferred)
{
    if (ec) 
    {
        cout << "Error occured! Error code = " << ec.value() << ". Message: " << ec.message();
        if (ec == asio::error::not_found) 
        {
             // No delimiter has been found in the
             // request message.
             status_code = 413;
             send_response();
             return;
        }
        else 
        {
            // In case of any other error –
            // close the socket and clean up.
            on_finish();
            return;
        }
    }

    // Parse the request line.
    std::string request_line;
    std::istream request_stream(&request);
    std::getline(request_stream, request_line, '\r');
    // Remove symbol '\n' from the buffer.
    request_stream.get();

    // Parse the request line.
    std::string request_method; 
    std::istringstream request_line_stream(request_line);
    request_line_stream >> request_method;

    if (request_method.compare("GET") != 0) 
    {
        // Unsupported method.
        status_code = 501;
        send_response();
        return;
    }
            


    request_line_stream >> url;

    string request_http_version;
    request_line_stream >> request_http_version;

    if (request_http_version.compare("HTTP/1.1") != 0) 
    {
        // Unsupported HTTP version or bad request.
            status_code = 505;
            send_response();
            return;
    }

    // At this point the request line is successfully
    // received and parsed. Now read the request headers.
    asio::async_read_until(*client_sock.get(),request,"\r\n\r\n", [this]( const boost::system::error_code& ec, size_t bytes_transferred)
    {
        on_headers_received(ec, bytes_transferred);
    });

    return;
}

void Service::on_headers_received(const boost::system::error_code& ec,size_t bytes_transferred)
{
    if (ec) 
    {
        cout << "Error occured! Error code = " << ec.value() << ". Message: " << ec.message();
        if (ec == asio::error::not_found) 
        {
            // No delimiter has been found in the
            // request message.
            status_code = 413; 
            send_response();
            return;
        }
        else 
        {
            // In case of any other error - close the
            // socket and clean up.
            on_finish();
            return;
        }
    }
    // Parse and store headers.
    std::istream request_stream(&request);
    string header_name, header_value;

    while (!request_stream.eof()) 
    {
        std::getline(request_stream, header_name, ':');
        if (!request_stream.eof()) 
        {
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

void Service::process_request() 
{
    // Read file.

    if(url.compare("/") == 0)
    {
        url = std::string("home.html");
    }

    std::string resource_file_path = std::string("/home/cyber/http/") + url;



    if (!boost::filesystem::exists(resource_file_path)) 
    {
        // Resource not found.
        status_code = 404;
        url = std::string("error.html");
        
    }

    resource_file_path = std::string("/home/cyber/http/") + url;

    std::ifstream resource_fstream(resource_file_path,std::ifstream::binary);
    if (!resource_fstream.is_open()) 
    {
        // Could not open file.
        // Something bad has happened.
        status_code = 500;
        return;
    }

    // Find out file size.
    resource_fstream.seekg(0, std::ifstream::end);
    resource_size_bytes = static_cast<size_t>(resource_fstream.tellg());
    m_resource_buffer.reset(new char[resource_size_bytes]);

    resource_fstream.seekg(std::ifstream::beg);
    resource_fstream.read(m_resource_buffer.get(),resource_size_bytes);
    m_response_headers += string("content-length") + ": " + to_string(resource_size_bytes) + "\r\n";
}

void Service::send_response() 
{
    client_sock->shutdown(asio::ip::tcp::socket::shutdown_receive);

    auto status_line = http_table_g.at(status_code);
    m_response_status_line = std::string("HTTP/1.1 ") + status_line + "\r\n";
    m_response_headers += "\r\n";
    std::vector<asio::const_buffer> response_buffers;
    response_buffers.push_back (asio::buffer(m_response_status_line));

    if (m_response_headers.length() > 0) 
    {
        response_buffers.push_back(asio::buffer(m_response_headers));
    }
    if (resource_size_bytes > 0) 
    {
        response_buffers.push_back(asio::buffer(m_resource_buffer.get(), resource_size_bytes));
    }
            
    // Initiate asynchronous write operation.
    asio::async_write(*client_sock.get(), response_buffers, [this] (const boost::system::error_code& ec, std::size_t bytes_transferred)
    {
        on_response_sent(ec, bytes_transferred);
    });
}

void Service::on_response_sent(const boost::system::error_code& ec, std::size_t bytes_transferred) 
{
    if (ec) 
    {
        std::cout << * "Error occured Error code = " << ec.value () << ". Message : " << ec.message () ;
    }
    client_sock->shutdown(asio::ip::tcp::socket::shutdown_both);
    on_finish () ;   
}

void Service::on_finish()
{
    delete this;
}
    


//// ACCEPTOR private bits /////

void Acceptor::InitAccept()
{
    std::shared_ptr<asio::ip::tcp::socket> sock(new asio::ip::tcp::socket(m_ios));
    m_acceptor.async_accept(*sock.get(), [this, sock] (const boost::system::error_code& error)
    {
        OnAccept(error, sock);
    });
}

void Acceptor::OnAccept(const boost::system::error_code&ec, std::shared_ptr<asio::ip::tcp::socket> sock)
{
    if (ec)
    {   
        std::cout<<"Error occured! Error Code = " << ec.value() << ". Message: " << ec.message();
    }
    else 
    {
        (new Service(sock))->start_handling();
    }

    if (!m_isStopped.load()) 
    {
        InitAccept();
    }
    else 
    {
        m_acceptor.close();
    }
}

