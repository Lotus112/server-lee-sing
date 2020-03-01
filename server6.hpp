#ifndef _SERVER6HEAD_
#define _SERVER6HEAD_

#include <boost/asio.hpp> 
#include <boost/filesystem.hpp>
#include <fstream>
#include <atomic>
#include <thread>
#include <iostream>

using namespace boost;
using namespace std;

class Service {
    static const std::map<unsigned int,string> http_table;

    public: 
        Service(std::shared_ptr<boost::asio::ip::tcp::socket> sock) :
            client_sock(sock),
            request(4096),
            status_code(200),
            resource_size_bytes(0)
        {};
        void start_handling() {
            asio::async_read_until(*client_sock.get(), request,"\r\n",[this]( const boost::system::error_code& ec, size_t bytes_transferred)
            {
                on_request_line_received(ec, bytes_transferred);
            });
        }
    
    private: 
        void on_request_line_received(const boost::system::error_code& ec, size_t bytes_transferred);
        void on_headers_received(const boost::system::error_code& ec,size_t bytes_transferred);
        void process_request();
        void send_response();
        void on_response_sent(const boost::system::error_code& ec, std::size_t bytes_transferred);
        void on_finish();
    
    private:
        std::shared_ptr<boost::asio::ip::tcp::socket> client_sock;
        boost::asio::streambuf request;
        std::map<string, string> m_request_headers;
        string url;

        std::unique_ptr<char[]> m_resource_buffer;
        unsigned int status_code;
        size_t resource_size_bytes; 
        string m_response_headers; 
        string m_response_status_line;
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
        void InitAccept();
        void OnAccept(const boost::system::error_code&ec, std::shared_ptr<asio::ip::tcp::socket> sock);
    
    private:
        asio::io_context&m_ios;
        asio::ip::tcp::acceptor m_acceptor;
        std::atomic<bool>m_isStopped;

};

class Server
{
    public:
        Server() 
        {
            m_work.reset(new asio::io_context::work(m_ios));
        }
        //Start Server here
        void Start(unsigned short port_num, unsigned int thread_pool_size)
        {
            assert(thread_pool_size > 0);
            //Create/start Acceptor
            acc.reset(new Acceptor(m_ios, port_num));
            acc->Start();

            //Specified number of threads and add to pool
            //seccomp(threads, isolation)

            for (unsigned int i = 0; i < thread_pool_size; i++)
            {
                std::unique_ptr<std::thread> th(new std::thread([this]()
                {
                    m_ios.run();
                }));
                m_thread_pool.push_back(std::move(th));
            }
        }

        //stopping server
        void Stop()
        {
            acc->Stop();
            m_ios.stop();

            for (auto& th : m_thread_pool)
            {
                th->join();
            }
        }

        private:
            asio::io_context m_ios;
            std::unique_ptr<asio::io_context::work>m_work;
            std::unique_ptr<Acceptor>acc;
            std::vector<std::unique_ptr<std::thread>>m_thread_pool;
};

#endif  // _SERVER6HEAD_