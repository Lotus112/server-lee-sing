#include "server6.hpp"

// This functions handles the http requests from the client (url)
void Service::http_request(const boost::system::error_code& ec, size_t bytes)
{
    //Points the logger to correct pointer
    std::shared_ptr<spdlog::logger> loggers;

    if (ec) 
    {
        //Logger is accessed to output if there is an error 'critical' 
        loggers = spdlog::get("Server");
        // Outputs the logs with correct critical message
        loggers->critical("Error Code: ", ec.value(), "Message: ", ec.message());

        if (ec == asio::error::not_found) 
        {
            //Logs critical code 413 Request is too large
             status_code = 413;
             loggers->warn("Error Code: 413");
             server_response_handle();
             return;
        }
        else 
        {
            // If there was an additional unexplained error the socket is closed from the server.
            cleanup();
            return;
        }
    }

    // This is to parse the request line 'r_line' from the clients input
    std::string r_line;
    std::istream r_stream(&request);
    std::getline(r_stream, r_line, '\r');
    // Removes symbol '\n' from the buffer to be parsed.
    r_stream.get();
    // Parse the request line to get the http method (POST) (GET).
    std::string r_method; 
    std::istringstream r_line_stream(r_line);
    r_line_stream >> r_method;
    

    //Only allows the GET method or the POST method any other method the server disconnects the client
    if (r_method.compare("GET") != 0 && r_method.compare("POST") != 0)  
    {
        status_code = 501;
        loggers = spdlog::get("Server");
        loggers->critical("Unsupported Method: Error 501");
        server_response_handle();
        return;
    }

    
    //Passes the line stream to the URL
    r_line_stream >> url;
    

    if(r_method.compare("POST") == 0)
    { 
        loggers = spdlog::get("Client");
        loggers->info("POST Data sent by client");
    }

    //Next is to parse the http version
    string request_http_version;
    r_line_stream >> request_http_version;

    if (request_http_version.compare("HTTP/1.1") != 0) 
    {
            status_code = 505;
            loggers->critical("Unsupported HTTP Version: Error 505");
            server_response_handle();
            return;
    }

    // Once the request has been parsed sucesfully the request header (http) is read
    asio::async_read_until(*client_sock.get(),request,"\r\n\r\n", [this]( const boost::system::error_code& ec, size_t bytes)
    {
        http_request_header(ec, bytes);
    });

    return;
}

//This function reads the request header
void Service::http_request_header(const boost::system::error_code& ec,std::size_t bytes)
{
    if (ec) 
    {
        std::shared_ptr<spdlog::logger> loggers;
        loggers = spdlog::get("Server");
        loggers->critical("Error Code: ", ec.value(), "Message: ", ec.message());
        if (ec == asio::error::not_found) 
        {
            loggers->warn("Error Code: 413");
            status_code = 413; 
            server_response_handle();
            return;
        }
        else 
        {
            //socket clean up
            cleanup();
            return;
        }
    }
    // Parse and store headers in appropriate values.
    std::istream r_stream(&request);

    std::string h_value, h_name;

    while (!r_stream.eof()) 
    {
        std::getline(r_stream, h_name, ':');
        if (!r_stream.eof()) 
        {
            std::getline(r_stream, h_value, '\r');
            // Remove symbol \n from the stream to pass through to request header.
            r_stream.get();
            request_header[h_name] = h_value;
        }
    }
    // Handle clients request
    http_request_handle();
    // Handle the servers response 
    server_response_handle();
    return;
}

void Service::http_request_handle() 
{
    std::shared_ptr<spdlog::logger> loggers;

    //checks for // input and parses it then passes the client to the home page
    if(url.compare("/") == 0 && url.compare("//"))
    {
        url = std::string("home.html");
        loggers = spdlog::get("Client");
        loggers->info("Client Accessed: Webpage ");
    }

    if(url.compare("///") == 0 && url.compare("////"))
    {
        url = std::string("home.html");
        loggers = spdlog::get("Client");
        loggers->info("Client Accessed: Webpage ");
    }

    //sets the file path to find the correlating 
    std::string file_path = std::string("/home/cyber/http/") + url;


    //if the file is not found it returns error 404 with the appropriate page
    if (!boost::filesystem::exists(file_path)) 
    {
        status_code = 404;
        loggers = spdlog::get("Client");
        loggers->warn("Page not found: Error 404 ");
        url = std::string("error.html");
        
    }


    file_path = std::string("/home/cyber/http/") + url;

    std::ifstream r_fstream(file_path,std::ifstream::binary);
    if (!r_fstream.is_open()) 
    {
        //If file can not open send out appropriate message along side log
        loggers = spdlog::get("Client");
        loggers->warn("Could not open file: Error 500 ");
        status_code = 500;
        return;
    }

    // Find out file size to pass into the http header "content length".
    r_fstream.seekg(0, std::ifstream::end);
    resource_size = static_cast<size_t>(r_fstream.tellg());
    r_buffer.reset(new char[resource_size]);

    // passes into the buffer 
    r_fstream.seekg(std::ifstream::beg);
    r_fstream.read(r_buffer.get(),resource_size);
    r_header += string("content-length") + ": " + to_string(resource_size) + "\r\n";
}


// Handles server response 
void Service::server_response_handle() 
{
    //Closes the socket
    client_sock->shutdown(asio::ip::tcp::socket::shutdown_receive);

    //Creates the http header with the status line and content length 
    auto status_line = http_table_g.at(status_code);
    status_line = std::string("HTTP/1.1 ") + status_line + "\r\n";
    r_header += "\r\n";
    std::vector<asio::const_buffer> response_buffers;
    response_buffers.push_back (asio::buffer(status_line));

    if (r_header.length() > 0) 
    {
        response_buffers.push_back(asio::buffer(r_header));
    }
    if (resource_size > 0) 
    {
        response_buffers.push_back(asio::buffer(r_buffer.get(), resource_size));
    }
            
    // Initiate asynchronous write operation to the client with the buffer and http header.
    asio::async_write(*client_sock.get(), response_buffers, [this] (const boost::system::error_code& ec, std::size_t bytes)
    {
        response_sent(ec, bytes);
    });
}

//Call back to check any errors before closing up the sockets and running cleanup
void Service::response_sent(const boost::system::error_code& ec, std::size_t bytes) 
{
    if (ec) 
    {
        std::shared_ptr<spdlog::logger> loggers;
        loggers = spdlog::get("Server");
        loggers->critical("Error Code: ", ec.value(), "Message: ", ec.message());
    }
    client_sock->shutdown(asio::ip::tcp::socket::shutdown_both);
    cleanup () ;   
}
//closes and deletes the instance of service object called
void Service::cleanup()
{
    delete this;
}
    


//// ACCEPTOR private bits /////

//Listens for connections then when accepted exectues on_accept
void Acceptor::Conn_Accept()
{
    std::shared_ptr<asio::ip::tcp::socket> client(new asio::ip::tcp::socket(ioc));
    c_acceptor.async_accept(*client.get(), [this, client] (const boost::system::error_code& error)
    {
        On_Accept(error, client);
    });
}


//Once accepeted the On_Accept starts the client handle which runs the service of the server and handles requests
//however if connection is closed then closes connections
void Acceptor::On_Accept(const boost::system::error_code&ec, std::shared_ptr<asio::ip::tcp::socket> client)
{
    if (ec)
    {   
        std::cout<<"Error occured! Error Code = " << ec.value() << ". Message: " << ec.message();
    }
    else 
    {
        
        //std::shared_ptr<spdlog::logger> loggers;
        //loggers = spdlog::get("Client");
        //loggers->info("Client Succesfully connected");

        (new Service(client))->client_handle();
    }

    if (!is_Stopped.load()) 
    {
        Conn_Accept();
    }
    else 
    {
        c_acceptor.close();
    }
}

