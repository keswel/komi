#include "raylib.h"
#include <iostream>
#include <vector>
#include <algorithm>

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

    float circleX = screenWidth  / 2.0f;
    float circleY = screenHeight / 2.0f;
    const float playerRadius = 15.0f;             // matches circleDiameter
    const float playerSpeed  = 400.0f;

    std::vector<Bullet> bullets;
    std::vector<Enemy>  enemies;
    std::string direction = "up";                 // default so first shot has a dir

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // player movement 
        if (IsKeyDown(KEY_W) && circleY - playerRadius > 0)                 { circleY -= playerSpeed * dt; direction = "up";    }
        else if (IsKeyDown(KEY_S) && circleY + playerRadius < screenHeight) { circleY += playerSpeed * dt; direction = "down";  }
        else if (IsKeyDown(KEY_A) && circleX - playerRadius > 0)            { circleX -= playerSpeed * dt; direction = "left";  }
        else if (IsKeyDown(KEY_D) && circleX + playerRadius < screenWidth)  { circleX += playerSpeed * dt; direction = "right"; }

        // spawn bullets / enemies 
        if (IsKeyPressed(KEY_SPACE)) {
            bullets.push_back({ {circleX, circleY}, 600.0f, direction });
        }
        if (IsKeyPressed(KEY_V)) {
            enemies.push_back({ {circleX, circleY}, 10.0f });
        }

        // update bullets 
        for (auto& b : bullets) {
            if      (b.direction == "up")    b.position.y -= b.speed * dt;
            else if (b.direction == "down")  b.position.y += b.speed * dt;
            else if (b.direction == "left")  b.position.x -= b.speed * dt;
            else if (b.direction == "right") b.position.x += b.speed * dt;
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

        // draw
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
