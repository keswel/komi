#include "raylib.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <boost/asio.hpp>
#include <sstream>
#include <unordered_map>
#include <mutex>

using boost::asio::ip::tcp;
tcp::socket* global_socket = nullptr;
std::string player_id;
bool player_id_received = false;

// thread-safe storage for other players
std::unordered_map<int, Vector2> other_players;
std::mutex other_players_mutex;
std::mutex enemies_mutex;

int player_score = 0;
int enemy_score = 0;
int scoreboard_fx_time = 0;
int shoot_fx_time = 0;

std::string selected_weapon = "pistol";

const int screenWidth  = 1280;
const int screenHeight = 720;

struct Bullet {
    Vector2 position;
    float speed = 1600.0f;
    std::string direction;
    static constexpr float RADIUS = 5.0f;         
};

struct Enemy {
    Vector2 position;
    float   speed = 10.0f;
    int client_id;          
    static constexpr float RADIUS = 15.0f;
};

std::vector<Bullet> bullets;
std::vector<Enemy>  enemies;

void send_to_server(const std::string& msg) {
    try {
        if (global_socket && global_socket->is_open()) {
            boost::asio::write(*global_socket, boost::asio::buffer(msg));
        } else {
            std::cerr << "Socket is not connected!" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "send_to_server failed: " << e.what() << '\n';
    }
}

void send_player_position(float circleX, float circleY) {
    std::string msg = "Position " + std::to_string(circleX) + ", " + std::to_string(circleY) + "\n";
    send_to_server(msg);
}

void send_bullet_position(Bullet bullet) {
    std::string msg = "Shot " + std::to_string(bullet.position.x) + " " + std::to_string(bullet.position.y) + " " + std::to_string(bullet.speed) + " " + bullet.direction + "\n";
    send_to_server(msg);
}

std::vector<std::string> split_by_space(std::string input) {
    std::istringstream iss(input);
    std::string word;
    std::vector<std::string> words;

    while (iss >> word) {
        words.push_back(word);
    }
    return words;
}

void create_enemy_for_player(int client_id, Vector2 position) {
    std::lock_guard<std::mutex> lock(enemies_mutex);
    
    // check if enemy already exists for this player
    for (const auto& enemy : enemies) {
        if (enemy.client_id == client_id) {
            return; // enemy already exists
        }
    }
    
    // create new enemy
    Enemy e{
        position,
        10.0f,
        client_id
    };
    enemies.push_back(std::move(e));
    std::cout << "Created enemy for player " << client_id << std::endl;
}

void update_enemy_position(int client_id, Vector2 position) {
    std::lock_guard<std::mutex> lock(enemies_mutex);
    for (Enemy& enemy : enemies) {
        if (enemy.client_id == client_id) {
            enemy.position = position;
            return;
        }
    }
}

void remove_enemy_for_player(int client_id) {
    std::lock_guard<std::mutex> lock(enemies_mutex);
    enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
        [client_id](const Enemy& e) {
            return e.client_id == client_id;
        }), enemies.end());
    std::cout << "Removed enemy for player " << client_id << std::endl;
}


void handle_client_id(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) return;
    player_id = tokens[1];
    player_id_received = true;
    std::cout << "PLAYER ID HAS BEEN SET TO " << player_id << std::endl;
}

void handle_bullet_update(const std::vector<std::string>& tokens) {
    if (tokens.size() < 6) return;

    try {
        float x = std::stof(tokens[1]);
        float y = std::stof(tokens[2]);
        std::string direction = tokens[3];
        float speed = std::stof(tokens[4]);
        float radius = std::stof(tokens[5]);

        static bool first_bullet_update = true;
        if (first_bullet_update) {
            bullets.clear();
            first_bullet_update = false;
        }

        bullets.push_back({{x, y}, speed, direction});

    } catch (const std::exception& e) {
        std::cerr << "Error parsing bullet message: " << e.what() << std::endl;
    }
}
void handle_hit(const std::vector<std::string>& tokens) {
    if (tokens.size() < 3) return;

    try {
        int shooter_id = std::stoi(tokens[1]);
        int hit_player_id = std::stoi(tokens[2]);

        if (std::to_string(shooter_id) == player_id) {
            player_score++;
            scoreboard_fx_time = 10;
            std::cout << "We hit player " << hit_player_id << "!" << std::endl;
        } else if (std::to_string(hit_player_id) == player_id) {
            enemy_score++;
            std::cout << "We were hit by player " << shooter_id << "!" << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error parsing hit message: " << e.what() << std::endl;
    }
}
void handle_client_position(const std::vector<std::string>& tokens) {
    if (tokens.size() < 5 || tokens[2] != "Position") return;

    std::string client_id_str = tokens[1];
    if (client_id_str.back() == ':') client_id_str.pop_back();

    try {
        int client_id = std::stoi(client_id_str);
        if (std::to_string(client_id) == player_id) return;

        std::string x_str = tokens[3];
        std::string y_str = tokens[4];
        if (x_str.back() == ',') x_str.pop_back();

        float x = std::stof(x_str);
        float y = std::stof(y_str);
        Vector2 position = {x, y};

        {
            std::lock_guard<std::mutex> lock(other_players_mutex);
            bool was_new_player = other_players.find(client_id) == other_players.end();
            other_players[client_id] = position;
        }

        update_enemy_position(client_id, position);
    } catch (const std::exception& e) {
        std::cerr << "Error parsing client position: " << e.what() << std::endl;
    }
}
void handle_player_event(const std::vector<std::string>& tokens) {
    if (tokens.size() < 3) return;

    const std::string& action = tokens[2];
    const std::string& player_id_str = tokens[1];

    if (action == "joined") {
        std::cout << "Player " << player_id_str << " joined the game" << std::endl;
    } else if (action == "left") {
        try {
            int client_id = std::stoi(player_id_str);
            {
                std::lock_guard<std::mutex> lock(other_players_mutex);
                other_players.erase(client_id);
            }
            remove_enemy_for_player(client_id);
            std::cout << "Player " << player_id_str << " left the game" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error parsing player leave event: " << e.what() << std::endl;
        }
    }
}
void parse_server_message(const std::string& message) {
    std::vector<std::string> tokens = split_by_space(message);
    if (tokens.empty()) return;

    const std::string& type = tokens[0];

    if (type == "Client_ID")         return handle_client_id(tokens);
    else if (type == "Bullet")       return handle_bullet_update(tokens);
    else if (type == "Hit")          return handle_hit(tokens);
    else if (type == "Client")       return handle_client_position(tokens);
    else if (type == "Player")       return handle_player_event(tokens);
}

void draw_weapons_selection() {
    std::string pistol_text = "1. pistol";
    std::string shotgun_text = "2. shotgun";
    int pistol_font = 20;
    int shotgun_font = 20;
    if (selected_weapon == "pistol") {
      pistol_font = 23;
    }else if (selected_weapon == "shotgun") {
      shotgun_font = 23;
    }

    //int fontSize = 20;
    int padding = 10;

    int lines = 2;

    // measure maximum line width
    int maxLineWidth = std::max(
        MeasureText(pistol_text.c_str(), pistol_font),
        MeasureText(shotgun_text.c_str(), shotgun_font)
    );

    int rectWidth = maxLineWidth + 2 * padding;
    int rectHeight = lines * 23 + 2 * padding;

    int rectX = 10;
    int rectY = screenHeight - rectHeight - 10;

    // draw background
    DrawRectangle(rectX, rectY, rectWidth, rectHeight, BLACK);

    // selected item (pure white)
    const Color COLOR_SELECTED   = { 255, 255, 255, 255 };   // R, G, B, A

    // un‑selected item (very light grey, ~15 % darker)
    const Color COLOR_UNSELECTED = { 220, 220, 220, 255 };

    // draw each line of text
    if (selected_weapon == "pistol") {
      DrawText(pistol_text.c_str(), rectX + padding, rectY + padding, pistol_font, COLOR_SELECTED);
      DrawText(shotgun_text.c_str(), rectX + padding, rectY + padding + shotgun_font, shotgun_font, COLOR_UNSELECTED);
    }else if (selected_weapon == "shotgun") {
      DrawText(pistol_text.c_str(), rectX + padding, rectY + padding, pistol_font, COLOR_UNSELECTED);
      DrawText(shotgun_text.c_str(), rectX + padding, rectY + padding + shotgun_font, shotgun_font+2, COLOR_SELECTED);
    }
}
void draw_scoreboard() {
    std::string scoreboard = std::to_string(player_score) + " | " + std::to_string(enemy_score);
    const char* scoreboard_text = scoreboard.c_str();
    int textWidth = MeasureText(scoreboard_text, 20);
    int rectWidth = textWidth + 40;
    int rectHeight = 40;
    int rectX = (screenWidth - rectWidth) / 2;
    int rectY = 10;

    if (scoreboard_fx_time != 0) {
      DrawRectangle(rectX, rectY, rectWidth, rectHeight, WHITE);
      scoreboard_fx_time--; 
    } else {
      DrawRectangleGradientH(rectX, rectY, rectWidth, rectHeight, LIGHTGRAY, RED);
    }
      DrawText(scoreboard_text, rectX + 20, rectY + 10, 20, RAYWHITE);
}

int main() {
    boost::asio::io_context io_context;
    tcp::resolver resolver(io_context);
    auto endpoints = resolver.resolve("127.0.0.1", "8080");
    tcp::socket socket(io_context);

    try {
        boost::asio::connect(socket, endpoints);
        global_socket = &socket;

        // spawn thread to read from server
        std::thread reader_thread([&socket]() {
            try {
                boost::asio::streambuf buf;
                while (true) {
                    boost::asio::read_until(socket, buf, "\n");
                    std::istream is(&buf);
                    std::string line;
                    std::getline(is, line);
                    parse_server_message(line);
                }
            } catch (std::exception& e) {
                std::cerr << "Server read error: " << e.what() << std::endl;
            }
        });
        reader_thread.detach();

    } catch (std::exception& e) {
        std::cerr << "Connection failed: " << e.what() << std::endl;
    }


    InitWindow(screenWidth, screenHeight, "komi");
    SetTargetFPS(240);
  

    float circleX = screenWidth  / 2.0f;
    float circleY = screenHeight / 2.0f;
    const float playerRadius = 15.0f;
    const float playerSpeed  = 400.0f;

    std::string direction = "up";
    std::string latest_right_direction = "up";

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        float dx = 0, dy = 0;

        if (IsKeyDown(KEY_W) && circleY - playerRadius > 0) dy = -1;
        if (IsKeyDown(KEY_S) && circleY + playerRadius < screenHeight) dy =  1;
        if (IsKeyDown(KEY_A) && circleX - playerRadius > 0) dx = -1;
        if (IsKeyDown(KEY_D) && circleX + playerRadius < screenWidth) dx =  1;

        // update facing direction
        if (IsKeyDown(KEY_W) && IsKeyDown(KEY_D)) {
            direction = "top_right";
        }
        else if (IsKeyDown(KEY_W) && IsKeyDown(KEY_A)) {
            direction = "top_left";
        }
        else if (IsKeyDown(KEY_S) && IsKeyDown(KEY_D)) {
            direction = "bottom_right";
        }
        else if (IsKeyDown(KEY_S) && IsKeyDown(KEY_A)) {
            direction = "bottom_left";
        }
        else if (IsKeyDown(KEY_W)) {
            direction = "up";
            latest_right_direction = direction;
        }
        else if (IsKeyDown(KEY_S)) {
            direction = "down";
            latest_right_direction = direction;
        }
        else if (IsKeyDown(KEY_A)) {
            direction = "left";
            latest_right_direction = direction;
        }
        else if (IsKeyDown(KEY_D)) {
            direction = "right";
            latest_right_direction = direction;
        }
        
        bool noKeysDown = 
            !IsKeyDown(KEY_W) &&
            !IsKeyDown(KEY_A) &&
            !IsKeyDown(KEY_S) &&
            !IsKeyDown(KEY_D) &&
            !IsKeyDown(KEY_SPACE);  
        if (noKeysDown) {
          direction = latest_right_direction; 
        }

        // update player position
        float len = std::sqrt(dx*dx + dy*dy);
        if (len > 0.0f) { 
          dx /= len;  
          dy /= len; 
        }
        circleX += dx * playerSpeed * dt;
        circleY += dy * playerSpeed * dt;
        
        // only send position if we have received our player ID
        if (player_id_received) {
            send_player_position(circleX, circleY);
        }
        
        if (IsKeyPressed(KEY_ONE)) {
          selected_weapon = "pistol";
        }

        if (IsKeyPressed(KEY_TWO)) {
          selected_weapon = "shotgun";
        }

        // shoot
        if (IsKeyPressed(KEY_SPACE)) {
            bullets.push_back({ {circleX, circleY}, 600.0f, direction });
            Bullet bullet{ {circleX, circleY}, 600.0f, direction };
            send_bullet_position(bullet);
        }
        
        // spawn enemy (temporary - for testing)
        if (IsKeyPressed(KEY_V)) {
            enemies.push_back({ {circleX, circleY}, 10.0f, -1 }); // -1 for local test enemies
        }

        // update bullets 
        for (auto& b : bullets) {
            if (b.direction == "top_right") {
                b.position.x += b.speed * dt;
                b.position.y -= b.speed * dt;
            }
            else if (b.direction == "top_left") {
                b.position.x -= b.speed * dt;
                b.position.y -= b.speed * dt;
            }
            else if (b.direction == "bottom_right") {
                b.position.x += b.speed * dt;
                b.position.y += b.speed * dt;
            }
            else if (b.direction == "bottom_left") {
                b.position.x -= b.speed * dt;
                b.position.y += b.speed * dt;
            }
            else if (b.direction == "up") {
                b.position.y -= b.speed * dt;
            }
            else if (b.direction == "down") {
                b.position.y += b.speed * dt;
            }
            else if (b.direction == "left") {
                b.position.x -= b.speed * dt;
            }
            else if (b.direction == "right") {
                b.position.x += b.speed * dt;
            }
        }

        // handle collisions with enemies
        for (auto bIt = bullets.begin(); bIt != bullets.end(); ) {
            bool removedBullet = false;

            {
                std::lock_guard<std::mutex> lock(enemies_mutex);
                for (auto eIt = enemies.begin(); eIt != enemies.end(); ) {
                    if (CheckCollisionCircles(bIt->position, Bullet::RADIUS,
                                              eIt->position, Enemy::RADIUS)) {

                        std::cout << "Hit enemy (player " << eIt->client_id << ")!" << std::endl;
                        eIt = enemies.erase(eIt);
                        bIt = bullets.erase(bIt);
                        removedBullet = true;

                        scoreboard_fx_time = 10;
                        player_score++;
                        send_to_server("Bullet has taken out enemy!\n");
                        break;
                    } else {
                        ++eIt;
                    }
                }
            }

            if (!removedBullet) ++bIt;
        }

        // delete off‑screen bullets 
        bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
            [&](const Bullet& b){
                return b.position.x < 0 || b.position.x > screenWidth ||
                       b.position.y < 0 || b.position.y > screenHeight;
            }), bullets.end());

        BeginDrawing();
        ClearBackground(DARKGRAY);

        draw_scoreboard();
        draw_weapons_selection();

        // draw current player (white circle)
        DrawCircleV({circleX, circleY}, playerRadius, WHITE);

        // draw other players (red circles)
        {
            std::lock_guard<std::mutex> lock(other_players_mutex);
            for (const auto& player : other_players) {
                DrawCircleV(player.second, playerRadius, RED);
            }
        }

        // draw bullets and enemies
        for (const auto& b : bullets) DrawCircleV(b.position, Bullet::RADIUS, PINK);
        {
            std::lock_guard<std::mutex> lock(enemies_mutex);
            for (const auto& e : enemies) {
                DrawCircleV(e.position, Enemy::RADIUS, ORANGE);
            }
        }

        EndDrawing();
    }
    
    CloseWindow();
    return 0;
}

