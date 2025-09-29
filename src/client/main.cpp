#include <raylib.h>
#include <cmath>

int main() {
    const int screenWidth = 960;
    const int screenHeight = 540;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, "R-Type Client (raylib)");
    SetTargetFPS(60);

    float t = 0.f;
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        t += dt;

        BeginDrawing();
        ClearBackground(BLACK);
        // very simple starfield: moving dots
        for (int i = 0; i < 200; ++i) {
            float x = fmodf((i * 73 + t * 60), (float)GetScreenWidth());
            float y = (i * 37) % GetScreenHeight();
            DrawPixel((int)x, (int)y, (i % 7 == 0) ? RAYWHITE : DARKGRAY);
        }
        DrawText("R-Type prototype - raylib client", 20, 20, 20, RAYWHITE);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
