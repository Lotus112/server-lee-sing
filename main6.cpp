#include "server6.hpp"


const unsigned int Default_thread_pool_size = 2;

int main()
{
    unsigned short port = 1333;

    try {
        Server srv;

        unsigned int thread_pool_size = std::thread::hardware_concurrency() * 2;

        if(thread_pool_size == 0)
        {
            thread_pool_size = Default_thread_pool_size;
        }

        srv.Start(port, thread_pool_size);

        //std::this_thread::sleep_for(std::chrono::seconds(60));
        while(true)
        {
            std::string Input;
            //Read Console input
            std::getline(std::cin, Input);
            //Process Input
            if(Input == "Stop")
            {
                srv.Stop();
                break;
            }
        }

    }
    catch (system::system_error&e)
    {
        std::cout << "Error Occured! Error Code: " << e.code() << ". Message: " << e.what();
    }

    return 0;
}
