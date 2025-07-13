#include <boost/asio.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <memory>

using boost::asio::ip::tcp;

struct ClientSession {
    std::shared_ptr<tcp::socket> socket;
    int client_id;
    
    ClientSession(std::shared_ptr<tcp::socket> sock, int id) 
        : socket(sock), client_id(id) {}
};

std::vector<ClientSession> clients;
std::mutex clients_mutex;

void broadcast_to_all(const std::string& message, int sender_id = -1) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    
    for (auto it = clients.begin(); it != clients.end(); ) {
        try {
            // Don't send message back to sender
            if (it->client_id != sender_id && it->socket->is_open()) {
                boost::asio::write(*it->socket, boost::asio::buffer(message));
                ++it;
            } else if (it->client_id == sender_id) {
                ++it; // Skip sender but keep them in the list
            } else {
                // Socket is closed, remove from list
                it = clients.erase(it);
            }
        } catch (const std::exception& e) {
            std::cerr << "Error broadcasting to client " << it->client_id << ": " << e.what() << std::endl;
            // Remove client that we can't send to
            it = clients.erase(it);
        }
    }
}

void remove_client(int client_id) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    clients.erase(std::remove_if(clients.begin(), clients.end(),
        [client_id](const ClientSession& client) {
            return client.client_id == client_id;
        }), clients.end());
}

void session(std::shared_ptr<tcp::socket> socket, int client_id) {
    try {
        std::cout << "Client " << client_id << " session started\n";
        boost::asio::streambuf buf;
        boost::system::error_code error;
        
        // Add client to the list
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.emplace_back(socket, client_id);
        }
        
        // Send client their ID
        std::string connection_id = "Client_ID " + std::to_string(client_id) + "\n";
        boost::asio::write(*socket, boost::asio::buffer(connection_id));
        
        // Notify other clients about new connection
        std::string join_message = "Player " + std::to_string(client_id) + " joined\n";
        broadcast_to_all(join_message, client_id);
        
        while (true) {
            size_t len = boost::asio::read_until(*socket, buf, "\n", error);
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
            
            // Log received data
            std::cout << "Client " << client_id << ": " << line << std::endl;
            
            // Broadcast the message to all other clients
            std::string broadcast_message = "Client " + std::to_string(client_id) + ": " + line + "\n";
            broadcast_to_all(broadcast_message, client_id);
        }
    } catch (std::exception& e) {
        std::cerr << "Exception in client " << client_id << " session: " << e.what() << std::endl;
    }
    
    // Clean up when client disconnects
    remove_client(client_id);
    
    // Notify other clients about disconnection
    std::string leave_message = "Player " + std::to_string(client_id) + " left\n";
    broadcast_to_all(leave_message, client_id);
}

int main() {
    try {
        boost::asio::io_context io_context;
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 8080));
        std::cout << "Server listening on port 8080...\n";
        
        int next_client_id = 0;
        
        while (true) {
            auto socket = std::make_shared<tcp::socket>(io_context);
            acceptor.accept(*socket);
            next_client_id++;
            int client_id = next_client_id;
            
            auto remote_ep = socket->remote_endpoint();
            std::cout << "Client " << client_id << " connected from "
                      << remote_ep.address().to_string() << ":"
                      << remote_ep.port() << std::endl;
            
            // Launch a thread for this client session
            std::thread(session, socket, client_id).detach();
        }
    } catch (std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
    }
    return 0;
}
