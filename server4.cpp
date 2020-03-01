#include <boost/asio.hpp> 
#include <boost/filesystem.hpp>
#include <fstream>
#include <memory>
#include <atomic>
#include <thread>
#include <iostream>

using namespace boost;
using namespace std;


class Service
{
    public:
        Service(std::shared_ptr<asio::ip::tcp::socket> sock) : m_sock(sock)
        {}

        void start_handling()
        {
            asio::async_read_until(*m_sock.get(),m_request,'\n',[this] (
                const boost::system::error_code& ec,std::size_t bytes_transferred){
                    onRequestReceived(ec, bytes_transferred);
                });
        }

    private:
        void onRequestReceived(const boost::system::error_code& ec, std::size_t bytes_transferred) 
        {
            if (ec) 
            {
                std::cout << "Error occured Error Code = " << ec.value() << ". Message : " << ec.message();
                onFinish();
                return;
            }
            // Process the request.
            m_response = ProcessRequest(m_request);
            std::cout << m_response << std::endl;
            // Initiate asynchronous write operation.
            asio::async_write(*m_sock.get(),asio::buffer(m_response),[this](
                const boost::system::error_code& ec,std::size_t bytes_transferred)
                {
                    onResponseSent (ec, bytes_transferred); 
                });
        }
        void onResponseSent(const boost::system::error_code& ec, std::size_t bytes_transferred)
        {
            if(ec)
            {
                std::cout << "Error occured Error Code = " << ec.value() << ". Message : " << ec.message();
            }

            onFinish();
        }

        void onFinish()
        {
            delete this;
        }

        std::string ProcessRequest(asio::streambuf& request){
            //method to parse the request and prepare the request

            //emulate cpu operations
            int i = 0;
            while (i != 1000000)
                i++;
            
            // emulate opertionas block the thread
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            const std::string response = 
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: 132\r\n\r\n"
            "<html>\r\n"
            "<head><title>404 Not Found</title></head>\r\n"
            "<body> Hello </body>\r\n"
            "</html>";
            return response;

        }
    
    private:
        std::shared_ptr<asio::ip::tcp::socket> m_sock;
        std::string m_response;
        asio::streambuf m_request;
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
                OnAccept(error, sock);
            });
        }

        void OnAccept(const boost::system::error_code&ec, std::shared_ptr<asio::ip::tcp::socket> sock)
        {
            if (ec)
            {   
                std::cout<<"Error occured! Error Code = " << ec.value() << ". Message: " << ec.message();
            }
            else {
                (new Service(sock))->start_handling();
            }

            if (!m_isStopped.load()) 
            {
                InitAccept();
            }
            else {
                m_acceptor.close();
            }
        }
    
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

const unsigned int Default_thread_pool_size = 2;

int main()
{
    unsigned short port_num = 1333;

    try {
        Server srv;

        unsigned int thread_pool_size = std::thread::hardware_concurrency() * 2;

        if(thread_pool_size == 0)
        {
            thread_pool_size = Default_thread_pool_size;
        }

        srv.Start(port_num, thread_pool_size);

        std::this_thread::sleep_for(std::chrono::seconds(60));

        srv.Stop();
    }
    catch (system::system_error&e)
    {
        std::cout << "Error Occured! Error Code: " << e.code() << ". Message: " << e.what();
    }

    return 0;
}
