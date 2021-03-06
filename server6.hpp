#ifndef _SERVER6HEAD_
#define _SERVER6HEAD_


#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/async.h"

#include <boost/asio.hpp> 
#include <boost/filesystem.hpp>

#include <fstream>
#include <atomic>
#include <thread>
#include <iostream>

using namespace boost;
using namespace std;

class Service {
    //This class handles function of the server to the clients
    //Allows the server to recieve requests, proccess it and respond with the appropriate message


    public: 
        // Pointer points to an instance of the soket connected to a client
        Service(std::shared_ptr<boost::asio::ip::tcp::socket> client) :
            client_sock(client), request(4096), status_code(200), resource_contents_size(0)
        {};
        // client_handle() initiates the communication with the client with the socket passed to the service class
        void client_handle() {
            asio::async_read_until(*client_sock.get(), request,"\r\n",[this]( const boost::system::error_code& ec, size_t bytes_transferred)
            {
                http_request(ec, bytes_transferred);
            });
        }
    
    //These private methods are to perform the receiving and processing of the requests made by the client
    //requests are parsed and executed 
    private: 
        void http_request(const boost::system::error_code& ec, std::size_t bytes_transferred);
        void http_request_header(const boost::system::error_code& ec,std::size_t bytes_transferred);
        void http_request_handle();
        void server_response_handle();
        void response_sent(const boost::system::error_code& ec, std::size_t bytes_transferred);
        void cleanup();
    
    private:
        std::shared_ptr<boost::asio::ip::tcp::socket> client_sock;
        boost::asio::streambuf request;
        std::map<string, string> m_request_headers;
        std::string url;

        std::unique_ptr<char[]> m_resource_buffer;
        unsigned int status_code;
        std::size_t resource_contents_size; 
        std::string m_response_headers; 
        std::string m_response_status_line;

        
};

const std::map<unsigned int, string>
        http_table_g =
    {
        { 200, "200 OK" },
        { 404, "404 Not Found" },
        { 413, "413 Request Entity Too Large" },
        { 500, "500 Server Error" },
        { 501, "501 Not Implemented" },
        { 505, "505 HTTP Version Not Supported" }
    };


class Acceptor {
    // Used for accepting new connections to the server and closing them also

    public:
        Acceptor(asio::io_context&ioc, unsigned short port) :
        ioc(ioc),
        m_acceptor(ioc, asio::ip::tcp::endpoint(asio::ip::address_v4::any(),port)), is_Stopped(false)
        {}

        // Start() instructs Acceptor class to start listening and accept incoming connections
        // m_acceptor listens for incoming connections
        void Start() 
        {
            m_acceptor.listen();
            Conn_Accept();
        }

        // Start() 
        void Stop()
        {
            is_Stopped.store(true);
        }

    private:

        // Constructs a socket and creates asynchrious accept operation
        // calls On_Accept() if connection successfuly or error occured

        void Conn_Accept();

        // A call back method that is called once the connections has been created succesfully
        // or there was an error. Once called the method calls the client_handle method in Service to start
        // handling the client.

        void On_Accept(const boost::system::error_code&ec, std::shared_ptr<asio::ip::tcp::socket> client);
    
    private:
        asio::io_context&ioc;
        asio::ip::tcp::acceptor m_acceptor;
        std::atomic<bool>is_Stopped;

};

class Server{
    // Runs the server when called ( Start() Stop ())

    public:
        Server() 
        {
            work_reset.reset(new asio::io_context::work(ioc));
        }
        //Start Server here
        void Start(unsigned short port, unsigned int thread_pool_size)
        {
            assert(thread_pool_size > 0);
            //Create / start Acceptor
            acc.reset(new Acceptor(ioc, port));
            acc->Start();

            //Specified number of threads and add to pool
            //seccomp(threads, isolation)

            for (unsigned int i = 0; i < thread_pool_size; i++)
            {
                std::unique_ptr<std::thread> th(new std::thread([this]()
                {
                    ioc.run();
                }));
                m_thread_pool.push_back(std::move(th));
            }
        }

        //stopping server
        void Stop()
        {
            acc->Stop();
            ioc.stop();

            for (auto& th : m_thread_pool)
            {
                th->join();
            }
        }

    private:
        asio::io_context ioc;
        std::unique_ptr<asio::io_context::work>work_reset;
        std::unique_ptr<Acceptor>acc;
        std::vector<std::unique_ptr<std::thread>>m_thread_pool;

        spdlog::init_thread_pool(8192, thread_pool_size);
        std::auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt >();
        std::auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("SystemLogs.txt", 1024*1024*10, 3);
        std::vector<spdlog::sink_ptr> sinks {stdout_sink, rotating_sink};
        std::auto logger = std::make_shared<spdlog::async_logger>("loggername", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
        spdlog::register_logger(logger);
};

#endif  // _SERVER6HEAD_