#include <boost/asio.hpp>
#include <iostream>

using boost::asio::ip::tcp;

int main() {
    try {
        boost::asio::io_context io_context;
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 8080));

        std::cout << "Server is listening on port 8080...\n";

        while (true) {
            tcp::socket socket(io_context);
            acceptor.accept(socket);
            std::cout << "Client connected.\n";

            try {
                // send greeting once
                std::string greeting = "Hello from server\n";
                boost::asio::write(socket, boost::asio::buffer(greeting));

                char data[1024];

                // read messages until client disconnects
                while (true) {
                    boost::system::error_code error;
                    size_t length = socket.read_some(boost::asio::buffer(data), error);

                    if (error == boost::asio::error::eof) {
                        // connection closed cleanly by peer
                        std::cout << "Client disconnected.\n";
                        break;
                    } else if (error) {
                        throw boost::system::system_error(error);
                    }

                    std::string client_message(data, length);
                    std::cout << "Received from client: " << client_message << std::endl;

                    // send a response back:
                    std::string response = "Message received\n";
                    boost::asio::write(socket, boost::asio::buffer(response));
                }
            }
            catch (std::exception& e) {
                std::cerr << "Communication error: " << e.what() << std::endl;
            }
        }
    }
    catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}
