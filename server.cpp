#include <boost/asio.hpp>
#include <iostream>
#include <thread>

using boost::asio::ip::tcp;

void session(tcp::socket socket, int client_id) {
    try {
        std::cout << "Client " << client_id << " session started\n";
        boost::asio::streambuf buf;
        boost::system::error_code error;

        while (true) {
            size_t len = boost::asio::read_until(socket, buf, "\n", error);
            if (error) {
                if (error == boost::asio::error::eof) {
                    std::cout << "Client " << client_id << " disconnected\n";
                } else {
                    std::cerr << "Error on client " << client_id << ": " << error.message() << std::endl;
                }
                break;
            }

            std::istream is(&buf);
            std::string line;
            std::getline(is, line);
            
            // data the server recieved
            std::cout << client_id << ": " << line << std::endl;
            
            // broadcast recieved data.
            std::string reply = client_id +"\n";
            boost::asio::write(socket, boost::asio::buffer(reply));
            //std::string reply = "Message received\n";
            //boost::asio::write(socket, boost::asio::buffer(reply));
        }
    } catch (std::exception& e) {
        std::cerr << "Exception in client " << client_id << " session: " << e.what() << std::endl;
    }
}

int main() {
    try {
        boost::asio::io_context io_context;

        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 8080));
        std::cout << "Server listening on port 8080...\n";

        int next_client_id = 0;

        while (true) {
            tcp::socket socket(io_context);
            acceptor.accept(socket);

            next_client_id++;
            int client_id = next_client_id;

            auto remote_ep = socket.remote_endpoint();
            std::cout << "Client " << client_id << " connected from "
                      << remote_ep.address().to_string() << ":"
                      << remote_ep.port() << std::endl;

            // launch a thread for this client session
            std::thread(session, std::move(socket), client_id).detach();
        }

    } catch (std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
    }

    return 0;
}
