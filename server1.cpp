#include <iostream>
#include <boost/asio.hpp>

using namespace boost::asio;
using ip::tcp;
using std::string;
using std::cout;
using std::endl;

string read_(tcp::socket & socket) {
       boost::asio::streambuf buf;
       boost::asio::read_until( socket, buf, "\n" );
       string data = boost::asio::buffer_cast<const char*>(buf.data());
       return data;
}
void send_(tcp::socket & socket, const string& message) {
       const string msg = message + "\n";
       boost::asio::write( socket, boost::asio::buffer(message) );
}

int main() {
    int Port;
    std::cin >> Port; 

    boost::asio::io_context io_context;
//listen for new connection  
    unsigned short port(Port);  
    tcp::acceptor acceptor_(io_context, tcp::endpoint(tcp::v4(), port )); 
    tcp::socket socket_(io_context);
    acceptor_.accept(socket_);

    string message = read_(socket_);
    cout << message << endl;
      
    send_(socket_, "Hello From Server!");
    cout << "Servent sent Hello message to Client!" << endl;

      

   return 0;
}
