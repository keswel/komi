#include "raylib.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>

struct Bullet {
    Vector2 position;
    float speed = 600.0f;
    std::string direction;
    static constexpr float RADIUS = 5.0f;         
};

struct Enemy {
    Vector2 position;
    float speed = 10.0f;
    static constexpr float RADIUS = 15.0f;        
};

int main() {
    const int screenWidth  = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "komi");
    SetTargetFPS(240);
  
    int player_score = 0;
    int enemy_score = 0;

    float circleX = screenWidth  / 2.0f;
    float circleY = screenHeight / 2.0f;
    const float playerRadius = 15.0f;             // matches circleDiameter
    const float playerSpeed  = 400.0f;

    std::vector<Bullet> bullets;
    std::vector<Enemy>  enemies;
    std::string direction = "up";                 // default so first shot has a dir

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        float dx = 0, dy = 0;

        if (IsKeyDown(KEY_W) && circleY - playerRadius > 0) dy = -1;
        if (IsKeyDown(KEY_S) && circleY + playerRadius < screenHeight) dy =  1;
        if (IsKeyDown(KEY_A) && circleX - playerRadius > 0) dx = -1;
        if (IsKeyDown(KEY_D) && circleX + playerRadius < screenWidth) dx =  1;

        // update facing direction once, on press
        if (IsKeyDown(KEY_W) && IsKeyDown(KEY_D)) {
            direction = "top right";
        }
        else if (IsKeyDown(KEY_W) && IsKeyDown(KEY_A)) {
            direction = "top left";
        }
        else if (IsKeyDown(KEY_S) && IsKeyDown(KEY_D)) {
            direction = "bottom right";
        }
        else if (IsKeyDown(KEY_S) && IsKeyDown(KEY_A)) {
            direction = "bottom left";
        }
        else if (IsKeyPressed(KEY_W)) {
            direction = "up";
        }
        else if (IsKeyPressed(KEY_S)) {
            direction = "down";
        }
        else if (IsKeyPressed(KEY_A)) {
            direction = "left";
        }
        else if (IsKeyPressed(KEY_D)) {
            direction = "right";
        }
        
        // update player position.
        float len = std::sqrt(dx*dx + dy*dy);
        if (len > 0.0f) { 
          dx /= len;  
          dy /= len; 
        }
        circleX += dx * playerSpeed * dt;
        circleY += dy * playerSpeed * dt;
        
        // shoot
        if (IsKeyPressed(KEY_SPACE)) {
            bullets.push_back({ {circleX, circleY}, 600.0f, direction });
        }
        // spawn enemy (temporary)
        if (IsKeyPressed(KEY_V)) {
            enemies.push_back({ {circleX, circleY}, 10.0f });
        }

        // update bullets 
        for (auto& b : bullets) {
            if (b.direction == "top right") {
                b.position.x += b.speed * dt;  // move right (x increasing)
                b.position.y -= b.speed * dt;  // move up (y decreasing)
            }
            else if (b.direction == "top left") {
                b.position.x -= b.speed * dt;  // move left
                b.position.y -= b.speed * dt;  // move up
            }
            else if (b.direction == "bottom right") {
                b.position.x += b.speed * dt;  // move right
                b.position.y += b.speed * dt;  // move down
            }
            else if (b.direction == "bottom left") {
                b.position.x -= b.speed * dt;  // move left
                b.position.y += b.speed * dt;  // move down
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

        // handle collisions 
        for (auto bIt = bullets.begin(); bIt != bullets.end(); ) {
            bool removedBullet = false;

            for (auto eIt = enemies.begin(); eIt != enemies.end(); ) {

                if (CheckCollisionCircles(bIt->position, Bullet::RADIUS,
                                          eIt->position, Enemy::RADIUS)) {
                    std::cout << "Collision!  Bullet "
                              << std::distance(bullets.begin(), bIt)
                              << " hit Enemy "
                              << std::distance(enemies.begin(), eIt) << '\n';

                    eIt = enemies.erase(eIt);     // kill enemy
                    bIt = bullets.erase(bIt);     // kill bullet
                    removedBullet = true;
                    player_score++;
                    break;                        // bullet is gone; break inner loop
                } else {
                    ++eIt;
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

        // draw scoreboard
        std::string scoreboard = std::to_string(player_score) + " | " + std::to_string(enemy_score);
        const char* scoreboard_text = scoreboard.c_str();
        int textWidth = MeasureText(scoreboard_text, 20);
        int rectWidth = textWidth + 40;  // padding
        int rectHeight = 40;
        int rectX = (screenWidth - rectWidth) / 2;
        int rectY = 10;
        
  
        DrawRectangle(rectX, rectY, rectWidth, rectHeight, DARKGRAY);
        DrawText(scoreboard_text, rectX + 20, rectY + 10, 20, RAYWHITE);
        BeginDrawing();
        ClearBackground(BLACK);

        DrawCircleV({circleX, circleY}, playerRadius, WHITE);

        for (const auto& b : bullets)  DrawCircleV(b.position, Bullet::RADIUS, PINK);
        for (const auto& e : enemies)  DrawCircleV(e.position, Enemy::RADIUS, RED);

        EndDrawing();
    }
    CloseWindow();
    return 0;
}
