#ifndef _SERVER6HEAD_
#define _SERVER6HEAD_

#include <unistd.h>
#include <seccomp.h>
#include <sys/prctl.h>

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
#include <string>

using namespace boost;
using namespace std;

class Service {
    //This class handles function of the server to the clients
    //Allows the server to recieve requests, proccess it and respond with the appropriate message


    public: 
        // Points to instance of the socket connected to a client
        Service(std::shared_ptr<boost::asio::ip::tcp::socket> client) :
            client_sock(client), request(4096), status_code(200), resource_size(0)
        {};
        // client_handle() initiates the communication with the client with the socket passed to the service class
        void client_handle() {
            asio::async_read_until(*client_sock.get(), request,"\r\n",[this]( const boost::system::error_code& ec, size_t bytes)
            {
                http_request(ec, bytes);
            });
        }
    
    //These private methods are to perform the receiving and processing of the requests made by the client
    //requests are parsed and executed 
    private: 
        void http_request(const boost::system::error_code& ec, std::size_t bytes);
        void http_request_header(const boost::system::error_code& ec,std::size_t bytes);
        void http_request_handle();
        void server_response_handle();
        void response_sent(const boost::system::error_code& ec, std::size_t bytes);
        void cleanup();
    
    // Private variables that are used within the class
    private:
        std::shared_ptr<boost::asio::ip::tcp::socket> client_sock;
        boost::asio::streambuf request;
        std::size_t resource_size;
        std::map<string, string> request_header;
        std::string r_header; 
        std::string url;
        std::unique_ptr<char[]> r_buffer;
        unsigned int status_code;
        std::string status_line;

        
};

//Map of status codes the server calls upon if there is an error
const std::map<unsigned int, string>
        http_table_g =
    {
        { 200, "200 OK" },
        { 404, "404 Not Found" },
        { 413, "413 Request Entity Is Too Large" },
        { 500, "500 Server Error" },
        { 501, "501 Not Implemented" },
        { 505, "505 HTTP Version Is Not Supported" }
    };


class Acceptor {
    // Used for accepting new connections to the server and closing them also

    public:
        Acceptor(asio::io_context&ioc, unsigned short port) :
        ioc(ioc),
        c_acceptor(ioc, asio::ip::tcp::endpoint(asio::ip::address_v4::any(),port)), is_Stopped(false)
        {}

        // Start() instructs Acceptor class to start listening and accept incoming connections
        // c_acceptor listens for incoming connections
        void Start() 
        {
            c_acceptor.listen();
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
        asio::ip::tcp::acceptor c_acceptor;
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

};

#endif  // _SERVER6HEAD_