#include <boost/asio.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <memory>
#include <unordered_map>
#include <sstream>
#include <cmath>
#include <algorithm>

using boost::asio::ip::tcp;

struct Vector2 {
    float x, y;
    Vector2(float x = 0, float y = 0) : x(x), y(y) {}
};

struct Player {
    int client_id;
    Vector2 position;
    float radius = 15.0f;
    int score = 0;
    
    Player() : client_id(0), position(0, 0) {}
    Player(int id, Vector2 pos) : client_id(id), position(pos) {}
};

struct Bullet {
    int owner_id;
    Vector2 position;
    float speed = 600.0f;
    std::string direction;
    static constexpr float RADIUS = 5.0f;
    
    Bullet(int owner, Vector2 pos, float spd, std::string dir) 
        : owner_id(owner), position(pos), speed(spd), direction(dir) {}
};

struct ClientSession {
    std::shared_ptr<tcp::socket> socket;
    int client_id;
    
    ClientSession(std::shared_ptr<tcp::socket> sock, int id) 
        : socket(sock), client_id(id) {}
};

// global game state
std::vector<ClientSession> clients;
std::unordered_map<int, Player> players;
std::vector<Bullet> bullets;
std::mutex clients_mutex;
std::mutex game_state_mutex;

// game constants
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 450;
const float TICK_RATE = 60.0f; // server tick rate
const float BULLET_LIFETIME = 5.0f; // seconds

bool check_collision_circles(Vector2 pos1, float radius1, Vector2 pos2, float radius2) {
    float dx = pos1.x - pos2.x;
    float dy = pos1.y - pos2.y;
    float distance = std::sqrt(dx * dx + dy * dy);
    return distance <= (radius1 + radius2);
}

void broadcast_to_all(const std::string& message, int sender_id = -1) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    
    for (auto it = clients.begin(); it != clients.end(); ) {
        try {
            // don't send message back to sender
            if (it->client_id != sender_id && it->socket->is_open()) {
                boost::asio::write(*it->socket, boost::asio::buffer(message));
                ++it;
            } else if (it->client_id == sender_id) {
                ++it; // skip sender but keep them in the list
            } else {
                // socket is closed, remove from list
                it = clients.erase(it);
            }
        } catch (const std::exception& e) {
            std::cerr << "Error broadcasting to client " << it->client_id << ": " << e.what() << std::endl;
            // remove client that we can't send to
            it = clients.erase(it);
        }
    }
}

void remove_client(int client_id) {
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.erase(std::remove_if(clients.begin(), clients.end(),
            [client_id](const ClientSession& client) {
                return client.client_id == client_id;
            }), clients.end());
    }
    
    {
        std::lock_guard<std::mutex> lock(game_state_mutex);
        players.erase(client_id);
        // remove bullets owned by this player
        bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
            [client_id](const Bullet& b) {
                return b.owner_id == client_id;
            }), bullets.end());
    }
}

std::vector<std::string> split_string(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    
    return tokens;
}

std::vector<std::string> split_by_space(const std::string& input) {
    std::istringstream iss(input);
    std::string word;
    std::vector<std::string> words;

    while (iss >> word) {
        words.push_back(word);
    }
    return words;
}

void update_bullet_position(Bullet& bullet, float dt) {
    Vector2 dir = {0, 0};

    if (bullet.direction == "top_right")        dir = { 1, -1 };
    else if (bullet.direction == "top_left")    dir = { -1, -1 };
    else if (bullet.direction == "bottom_right")dir = { 1, 1 };
    else if (bullet.direction == "bottom_left") dir = { -1, 1 };
    else if (bullet.direction == "up")          dir = { 0, -1 };
    else if (bullet.direction == "down")        dir = { 0, 1 };
    else if (bullet.direction == "left")        dir = { -1, 0 };
    else if (bullet.direction == "right")       dir = { 1, 0 };

    bullet.position.x += dir.x * bullet.speed * dt;
    bullet.position.y += dir.y * bullet.speed * dt;
}

void process_collisions() {
    std::lock_guard<std::mutex> lock(game_state_mutex);
    
    for (auto bullet_it = bullets.begin(); bullet_it != bullets.end();) {
        bool bullet_removed = false;
        
        // Check collision with all players except the bullet owner
        for (auto& [player_id, player] : players) {
            if (player_id != bullet_it->owner_id) {
                if (check_collision_circles(bullet_it->position, Bullet::RADIUS,
                                          player.position, player.radius)) {
                    
                    // player hit! Update scores - use find() instead of []
                    auto owner_it = players.find(bullet_it->owner_id);
                    if (owner_it != players.end()) {
                        owner_it->second.score++;
                    }
                    
                    // broadcast hit message
                    std::string hit_msg = "Hit " + std::to_string(bullet_it->owner_id) + 
                                         " " + std::to_string(player_id) + "\n";
                    broadcast_to_all(hit_msg);
                    
                    // remove bullet
                    bullet_it = bullets.erase(bullet_it);
                    bullet_removed = true;
                    break;
                }
            }
        }
        
        if (!bullet_removed) {
            // check if bullet is out of bounds
            if (bullet_it->position.x < 0 || bullet_it->position.x > SCREEN_WIDTH ||
                bullet_it->position.y < 0 || bullet_it->position.y > SCREEN_HEIGHT) {
                bullet_it = bullets.erase(bullet_it);
            } else {
                ++bullet_it;
            }
        }
    }
}

void game_loop() {
    auto last_time = std::chrono::high_resolution_clock::now();
    
    while (true) {
        auto current_time = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(current_time - last_time).count();
        last_time = current_time;
        
        // update bullets
        {
            std::lock_guard<std::mutex> lock(game_state_mutex);
            for (auto& bullet : bullets) {
                update_bullet_position(bullet, dt);
            }
        }
        
        // process collisions
        process_collisions();
        
        // broadcast bullet positions to all clients
        {
            std::lock_guard<std::mutex> lock(game_state_mutex);
            for (const auto& bullet : bullets) {
                std::string bullet_msg = "Bullet " + std::to_string(bullet.position.x) + 
                                       " " + std::to_string(bullet.position.y) + 
                                       " " + bullet.direction + 
                                       " " + std::to_string(bullet.speed) + 
                                       " " + std::to_string(Bullet::RADIUS) + "\n";
                broadcast_to_all(bullet_msg);
            }
        }
        
        // sleep to maintain tick rate
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(1000.0f / TICK_RATE)));
    }
}

void handle_client_message(const std::string& message, int client_id) {
    std::vector<std::string> tokens = split_by_space(message);
    
    if (tokens.empty()) return;
    
    if (tokens[0] == "Position" && tokens.size() >= 3) {
        try {
            // parse position: "Position x, y"
            std::string x_str = tokens[1];
            std::string y_str = tokens[2];
            
            // remove comma from x coordinate
            if (x_str.back() == ',') {
                x_str.pop_back();
            }
            
            float x = std::stof(x_str);
            float y = std::stof(y_str);
            
            // update player position
            {
                std::lock_guard<std::mutex> lock(game_state_mutex);
                auto player_it = players.find(client_id);
                if (player_it != players.end()) {
                    player_it->second.position = Vector2(x, y);
                } else {
                    players[client_id] = Player(client_id, Vector2(x, y));
                }
            }
            
            // broadcast to other clients
            std::string broadcast_message = "Client " + std::to_string(client_id) + ": " + message + "\n";
            broadcast_to_all(broadcast_message, client_id);
            
        } catch (const std::exception& e) {
            std::cerr << "Error parsing position from client " << client_id << ": " << e.what() << std::endl;
        }
    }
    else if (tokens[0] == "Shot" && tokens.size() >= 5) {
        try {
            // parse bullet: "Shot x y speed direction"
            float x = std::stof(tokens[1]);
            float y = std::stof(tokens[2]);
            float speed = std::stof(tokens[3]);
            std::string direction = tokens[4];
            
            // add bullet to server state
            {
                std::lock_guard<std::mutex> lock(game_state_mutex);
                bullets.emplace_back(client_id, Vector2(x, y), speed, direction);
            }
            
            std::cout << "Client " << client_id << " fired bullet at (" << x << ", " << y << ") direction: " << direction << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "Error parsing shot from client " << client_id << ": " << e.what() << std::endl;
        }
    }
    else {
        // for other messages, just broadcast them
        std::string broadcast_message = "Client " + std::to_string(client_id) + ": " + message + "\n";
        broadcast_to_all(broadcast_message, client_id);
    }
}

void session(std::shared_ptr<tcp::socket> socket, int client_id) {
    try {
        std::cout << "Client " << client_id << " session started\n";
        boost::asio::streambuf buf;
        boost::system::error_code error;
        
        // add client to the list
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.emplace_back(socket, client_id);
        }
        
        // initialize player
        {
            std::lock_guard<std::mutex> lock(game_state_mutex);
            players[client_id] = Player(client_id, Vector2(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2));
        }
        
        // send client their ID
        std::string connection_id = "Client_ID " + std::to_string(client_id) + "\n";
        boost::asio::write(*socket, boost::asio::buffer(connection_id));
        
        // notify other clients about new connection
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
            
            // log received data (unless it is position data)
            if (line.find("Position") == std::string::npos) {
                std::cout << "Client " << client_id << ": " << line << std::endl;
            }
            
            // handle the message
            handle_client_message(line, client_id);
        }
    } catch (std::exception& e) {
        std::cerr << "Exception in client " << client_id << " session: " << e.what() << std::endl;
    }
    
    // clean up when client disconnects
    remove_client(client_id);
    
    // notify other clients about disconnection
    std::string leave_message = "Player " + std::to_string(client_id) + " left\n";
    broadcast_to_all(leave_message, client_id);
}

int main() {
    try {
        boost::asio::io_context io_context;
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 8080));
        std::cout << "Server listening on port 8080...\n";
        
        // start game loop thread
        std::thread game_thread(game_loop);
        game_thread.detach();
        
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
            
            // launch a thread for this client session
            std::thread(session, socket, client_id).detach();
        }
    } catch (std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
    }
    return 0;
}
