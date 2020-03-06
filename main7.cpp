#include "server6.hpp"


const unsigned int Default_thread_pool_size = 5;

void LogCreate(unsigned int thread_pool_size)
{
    try
    {
        //Intilises the log thread pool
        spdlog::init_thread_pool(8192, thread_pool_size);

        //Sets the sink with colours
        auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt >();
        //Sets sink size alognside maximum amount of files
        auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("SystemLogs.txt", 1024*1024*10, 3);
        std::vector<spdlog::sink_ptr> sinks {stdout_sink, rotating_sink};

        // Assigns boths Server Log and Client Log to the same output file "SystemsLogs.txt"
        auto Server_logger = std::make_shared<spdlog::async_logger>("Server", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
        auto Client_logger = std::make_shared<spdlog::async_logger>("Client", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);

        //Registers the logs to be accesssed Globally 
        spdlog::register_logger(Server_logger);
        spdlog::register_logger(Client_logger);
    }
    catch (const spdlog::spdlog_ex& ex)
    {
        std::cout << "Log Creation Failed " << ex.what() << std::endl;
    }

}

int main()
{
    prctl(PR_SET_NO_NEW_PRIVS, 1);
    prctl(PR_SET_DUMPABLE, 0);

    // Init the filter
    scmp_filter_ctx ctx;
    ctx = seccomp_init(SCMP_ACT_ALLOW); // default action: kill

  // setup basic whitelist
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(rt_sigreturn), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(read), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(write), 0);
  
  // setup our rule
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(dup2), 2, 
                        SCMP_A0(SCMP_CMP_EQ, 1),
                        SCMP_A1(SCMP_CMP_EQ, 2));

  // build and load the filter
    seccomp_load(ctx);



    int Port;
    std::cout << "Please Enter a Port Number: " << std::endl;
    std::cin >> Port;
    unsigned short port(Port);
    try 
    {
        Server srv;

        unsigned int thread_pool_size = std::thread::hardware_concurrency() * 2;

        if(thread_pool_size == 0)
        {
            thread_pool_size = Default_thread_pool_size;
        }
        
        

        LogCreate(thread_pool_size);
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
    spdlog::shutdown();
    return 0;
}
