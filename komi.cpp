#include "raylib.h"
#include <iostream>
#include <vector>
#include <algorithm>

struct Bullet {
    Vector2 position;
    float speed = 600.0f;
    std::string direction;
};

struct Enemy {
  Vector2 position;
  float speed = 10.0f;
};

int main() {
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "komi");
    SetTargetFPS(240);

    int circleDiameter = 15;
    float circleX = screenWidth / 2.0f;
    float circleY = screenHeight / 2.0f;
    float speed = 400.0f;

    std::vector<Bullet> bullets;
    std::vector<Enemy> enemies;
    std::string direction;

    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();

        // Player movement
        if (IsKeyDown(KEY_W) && (circleY - circleDiameter > 0)) {
            circleY -= speed * deltaTime;
            direction = "up";
        }
        else if (IsKeyDown(KEY_S) && (circleY + circleDiameter < screenHeight)) {
            circleY += speed * deltaTime;
            direction = "down";
        }
        else if (IsKeyDown(KEY_A) && (circleX - circleDiameter > 0)) {
            circleX -= speed * deltaTime;
            direction = "left";
        }
        else if (IsKeyDown(KEY_D) && (circleX + circleDiameter < screenWidth)) {
            circleX += speed * deltaTime;
            direction = "right";
        }

        std::cout << direction << std::endl;
        // Shoot a bullet
        if (IsKeyPressed(KEY_SPACE)) {
            Bullet newBullet;
            newBullet.position = { circleX, circleY };
            newBullet.direction = direction; // store bullet direction
            bullets.push_back(newBullet);
        }

        if (IsKeyPressed(KEY_V)) {
          Enemy newEnemy;
          newEnemy.position = {circleX, circleY};
          enemies.push_back(newEnemy);
        }

        // Update bullet positions
        for (auto& bullet : bullets) {
            if (bullet.direction == "up") {
              bullet.position.y -= bullet.speed * deltaTime;
            }else if (bullet.direction == "down") {
              bullet.position.y += bullet.speed * deltaTime;
            }else if (bullet.direction == "right") {
              bullet.position.x += bullet.speed * deltaTime;
            }else if (bullet.direction == "left") {
              bullet.position.x -= bullet.speed * deltaTime;
            }
        }

        // Update Enemy
        for (auto& enemy : enemies) {

        }

        // Remove bullets that go off screen
        bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
            [&](Bullet& b) { return b.position.y < 0; }), bullets.end());

        // Drawing
        BeginDrawing();
        ClearBackground(BLACK);

        // Player
        DrawCircle((int)circleX, (int)circleY, circleDiameter, WHITE);

        // Bullets
        for (const auto& bullet : bullets) {
            DrawCircleV(bullet.position, 5.0f, PINK);
        }

        for (const auto& enemy : enemies) {
            DrawCircleV(enemy.position, 15.0f, RED);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
