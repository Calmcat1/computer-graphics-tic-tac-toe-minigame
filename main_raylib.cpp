#include "raylib.h"
#include "raymath.h"
#include <cmath>
#include <algorithm>
#include <cstring>

// ============================================================================
// STRICT NES (2C02) COLOR PALETTE
// ============================================================================
#define NES_BLACK       CLITERAL(Color){ 15,  15,  15,  255 } // $0F
#define NES_DARK_GRAY   CLITERAL(Color){ 44,  44,  44,  255 } // $00 (Dark)
#define NES_LIGHT_GRAY  CLITERAL(Color){ 188, 188, 188, 255 } // $10
#define NES_WHITE       CLITERAL(Color){ 252, 252, 252, 255 } // $30
#define NES_RED         CLITERAL(Color){ 216, 40,  0,   255 } // $16 (Player X)
#define NES_RED_LIGHT   CLITERAL(Color){ 252, 116, 96,  255 } // $26 (X Highlight)
#define NES_BLUE        CLITERAL(Color){ 0,   112, 236, 255 } // $12 (Player O)
#define NES_CYAN        CLITERAL(Color){ 60,  188, 252, 255 } // $21 (O Highlight)
#define NES_GREEN       CLITERAL(Color){ 0,   168, 0,   255 } // $1A (Win Line)
#define NES_YELLOW      CLITERAL(Color){ 252, 216, 168, 255 } // $38 (Draw/Hover)

enum CellState { EMPTY = 0, PLAYER_X = 1, PLAYER_O = 2 };
enum WinType { WIN_NONE = 0, WIN_ROW, WIN_COL, WIN_DIAG_MAIN, WIN_DIAG_ANTI };

struct CellAnimation {
    float progress = 0.0f; // 0.0 to 1.0
};

// Quantized Ease-Out Back: gives a springy pop-in snapped to retro steps
float RetroEaseOutBack(float x) {
    // Quantize time into 12 discrete animation frames for an authentic 8-bit feel
    float stepped = floorf(x * 12.0f) / 12.0f;
    if (x >= 0.99f) stepped = 1.0f;

    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    return 1.0f + c3 * powf(stepped - 1.0f, 3.0f) + c1 * powf(stepped - 1.0f, 2.0f);
}

// ============================================================================
// PROCEDURAL 16x16 PIXEL ART GENERATOR
// ============================================================================
Texture2D CreatePixelSpriteX() {
    // 16x16 pixel map: 0 = Transparent, 1 = Primary Red, 2 = Light Highlight
    const char* map[16] = {
        "................",
        ".21..........21.",
        ".211........211.",
        "..211......211..",
        "...211....211...",
        "....211..211....",
        ".....211211.....",
        "......2111......",
        "......2111......",
        ".....211211.....",
        "....211..211....",
        "...211....211...",
        "..211......211..",
        ".211........211.",
        ".21..........21.",
        "................"
    };

    Image img = GenImageColor(16, 16, BLANK);
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            if (map[y][x] == '1') ImageDrawPixel(&img, x, y, NES_RED);
            else if (map[y][x] == '2') ImageDrawPixel(&img, x, y, NES_RED_LIGHT);
        }
    }

    Texture2D tex = LoadTextureFromImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT); // Crisp pixel art scaling
    UnloadImage(img);
    return tex;
}

Texture2D CreatePixelSpriteO() {
    // 16x16 pixel map: 0 = Transparent, 1 = Primary Blue, 2 = Cyan Highlight
    const char* map[16] = {
        "................",
        ".....222211.....",
        "...2221111111...",
        "..2211....1111..",
        "..211......111..",
        ".221........111.",
        ".21..........11.",
        ".21..........11.",
        ".21..........11.",
        ".21..........11.",
        ".211........111.",
        "..111......111..",
        "..1111....1111..",
        "...1111111111...",
        ".....111111.....",
        "................"
    };

    Image img = GenImageColor(16, 16, BLANK);
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            if (map[y][x] == '1') ImageDrawPixel(&img, x, y, NES_BLUE);
            else if (map[y][x] == '2') ImageDrawPixel(&img, x, y, NES_CYAN);
        }
    }

    Texture2D tex = LoadTextureFromImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT); // Crisp pixel art scaling
    UnloadImage(img);
    return tex;
}

// ============================================================================
// PROCEDURAL 8-BIT SQUARE WAVE SOUND GENERATOR
// ============================================================================
Sound CreateRetroBeep(float startFreq, float endFreq, float duration) {
    int sampleRate = 44100;
    int sampleCount = (int)(sampleRate * duration);
    short* samples = (short*)RL_MALLOC(sampleCount * sizeof(short));

    float phase = 0.0f;
    for (int i = 0; i < sampleCount; i++) {
        float t = (float)i / sampleCount;
        float freq = Lerp(startFreq, endFreq, t);
        phase += freq / sampleRate;
        if (phase >= 1.0f) phase -= 1.0f;

        // Square wave with simple decay envelope
        float envelope = (1.0f - t);
        float square = (phase < 0.5f) ? 1.0f : -1.0f;
        samples[i] = (short)(square * envelope * 4000.0f);
    }

    Wave wave = {
        .frameCount = (unsigned int)sampleCount,
        .sampleRate = (unsigned int)sampleRate,
        .sampleSize = 16,
        .channels = 1,
        .data = samples
    };

    Sound sound = LoadSoundFromWave(wave);
    UnloadWave(wave); // Frees samples buffer
    return sound;
}

// ============================================================================
// MAIN GAME CLASS
// ============================================================================
class RetroTicTacToe {
public:
    CellState board[3][3] = {};
    CellAnimation anims[3][3] = {};

    int turn = PLAYER_X;
    int winner = 0; // 0: Ongoing, 1: X, 2: O, 3: Draw
    WinType winType = WIN_NONE;
    int winIndex = -1;

    float gridAnimProgress = 0.0f;
    float winLineAnimProgress = 0.0f;
    bool soundEnabled = true;

    // 16x16 tiles scaled by 9x = 144px per cell (432x432 board)
    const int pixelScale = 9;
    const float cellSize = 16.0f * 9.0f; // 144.0f
    const float boardSize = 144.0f * 3.0f; // 432.0f
    Vector2 boardOffset = { 84.0f, 160.0f };

    Texture2D texX;
    Texture2D texO;
    Sound sfxPlaceX;
    Sound sfxPlaceO;
    Sound sfxWin;
    Sound sfxReset;

    RetroTicTacToe() {
        texX = CreatePixelSpriteX();
        texO = CreatePixelSpriteO();

        sfxPlaceX = CreateRetroBeep(440.0f, 587.33f, 0.09f); // A4 -> D5
        sfxPlaceO = CreateRetroBeep(587.33f, 440.0f, 0.09f); // D5 -> A4
        sfxWin    = CreateRetroBeep(523.25f, 1046.5f, 0.35f); // C5 -> C6
        sfxReset  = CreateRetroBeep(330.0f, 220.0f, 0.12f);  // E4 -> A3

        Reset(false);
    }

    ~RetroTicTacToe() {
        UnloadTexture(texX);
        UnloadTexture(texO);
        UnloadSound(sfxPlaceX);
        UnloadSound(sfxPlaceO);
        UnloadSound(sfxWin);
        UnloadSound(sfxReset);
    }

    void Reset(bool playAudio = true) {
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                board[r][c] = EMPTY;
                anims[r][c] = { 0.0f };
            }
        }
        turn = PLAYER_X;
        winner = 0;
        winType = WIN_NONE;
        winIndex = -1;
        gridAnimProgress = 0.0f;
        winLineAnimProgress = 0.0f;

        if (playAudio && soundEnabled) PlaySound(sfxReset);
    }

    void Update(float dt) {
        if (IsKeyPressed(KEY_M)) soundEnabled = !soundEnabled;
        if (IsKeyPressed(KEY_R)) Reset(true);

        // Stepped grid entrance animation
        if (gridAnimProgress < 1.0f) {
            gridAnimProgress = std::min(1.0f, gridAnimProgress + dt * 3.5f);
        }

        // Piece pop-in animation
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                if (board[r][c] != EMPTY && anims[r][c].progress < 1.0f) {
                    anims[r][c].progress = std::min(1.0f, anims[r][c].progress + dt * 4.5f);
                }
            }
        }

        // Win strike line animation
        if (winner == PLAYER_X || winner == PLAYER_O) {
            if (winLineAnimProgress < 1.0f) {
                winLineAnimProgress = std::min(1.0f, winLineAnimProgress + dt * 4.0f);
            }
        }

        // Mouse Click Input
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && winner == 0 && gridAnimProgress >= 0.85f) {
            Vector2 mousePos = GetMousePosition();
            int col = (int)((mousePos.x - boardOffset.x) / cellSize);
            int row = (int)((mousePos.y - boardOffset.y) / cellSize);

            if (row >= 0 && row < 3 && col >= 0 && col < 3 && board[row][col] == EMPTY) {
                board[row][col] = (CellState)turn;

                if (soundEnabled) {
                    PlaySound((turn == PLAYER_X) ? sfxPlaceX : sfxPlaceO);
                }

                CheckWinner();
                if (winner == PLAYER_X || winner == PLAYER_O) {
                    if (soundEnabled) PlaySound(sfxWin);
                } else if (winner == 0) {
                    turn = (turn == PLAYER_X) ? PLAYER_O : PLAYER_X;
                }
            }
        }
    }

    void CheckWinner() {
        for (int i = 0; i < 3; i++) {
            if (board[i][0] != EMPTY && board[i][0] == board[i][1] && board[i][1] == board[i][2]) {
                winner = board[i][0];
                winType = WIN_ROW;
                winIndex = i;
                return;
            }
            if (board[0][i] != EMPTY && board[0][i] == board[1][i] && board[1][i] == board[2][i]) {
                winner = board[0][i];
                winType = WIN_COL;
                winIndex = i;
                return;
            }
        }

        if (board[0][0] != EMPTY && board[0][0] == board[1][1] && board[1][1] == board[2][2]) {
            winner = board[0][0];
            winType = WIN_DIAG_MAIN;
            return;
        }
        if (board[0][2] != EMPTY && board[0][2] == board[1][1] && board[1][1] == board[2][0]) {
            winner = board[0][2];
            winType = WIN_DIAG_ANTI;
            return;
        }

        bool emptyLeft = false;
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                if (board[r][c] == EMPTY) emptyLeft = true;
            }
        }
        if (!emptyLeft) winner = 3;
    }

    void Draw() {
        DrawRetroHeader();
        DrawPixelGrid();
        DrawPixelHover();
        DrawPixelPieces();
        DrawPixelWinLine();
        DrawFooterControls();
    }

private:
    void DrawShadowText(const char* text, int x, int y, int fontSize, Color color) {
        DrawText(text, x + 3, y + 3, fontSize, NES_DARK_GRAY);
        DrawText(text, x, y, fontSize, color);
    }

    void DrawRetroHeader() {
        const char* title = "TIC-TAC-TOE";
        DrawShadowText(title, 300 - MeasureText(title, 32) / 2, 36, 32, NES_WHITE);

        const char* status = "";
        Color statusColor = NES_WHITE;

        if (winner == 1) {
            status = "1UP (X) WINS! PRESS [R]";
            statusColor = NES_RED_LIGHT;
        } else if (winner == 2) {
            status = "2UP (O) WINS! PRESS [R]";
            statusColor = NES_CYAN;
        } else if (winner == 3) {
            status = "DRAW GAME! PRESS [R]";
            statusColor = NES_YELLOW;
        } else {
            status = (turn == PLAYER_X) ? "TURN: 1UP [X]" : "TURN: 2UP [O]";
            statusColor = (turn == PLAYER_X) ? NES_RED_LIGHT : NES_CYAN;
        }

        DrawShadowText(status, 300 - MeasureText(status, 20) / 2, 96, 20, statusColor);
    }

    void DrawPixelGrid() {
        // Snap line growth to pixel increments
        float rawLen = boardSize * std::min(1.0f, gridAnimProgress);
        int lineLen = ((int)rawLen / pixelScale) * pixelScale;
        int thickness = pixelScale; // 9px thick crisp pixel lines

        // Outer retro double-frame border
        DrawRectangleLinesEx(
            (Rectangle){ boardOffset.x - 18, boardOffset.y - 18, boardSize + 36, boardSize + 36 },
            (float)pixelScale,
            NES_DARK_GRAY
        );

        // Vertical grid bars
        DrawRectangle((int)(boardOffset.x + cellSize - thickness / 2), (int)boardOffset.y, thickness, lineLen, NES_LIGHT_GRAY);
        DrawRectangle((int)(boardOffset.x + cellSize * 2 - thickness / 2), (int)boardOffset.y, thickness, lineLen, NES_LIGHT_GRAY);

        // Horizontal grid bars
        DrawRectangle((int)boardOffset.x, (int)(boardOffset.y + cellSize - thickness / 2), lineLen, thickness, NES_LIGHT_GRAY);
        DrawRectangle((int)boardOffset.x, (int)(boardOffset.y + cellSize * 2 - thickness / 2), lineLen, thickness, NES_LIGHT_GRAY);
    }

    void DrawPixelHover() {
        if (winner != 0 || gridAnimProgress < 0.85f) return;

        Vector2 mousePos = GetMousePosition();
        int col = (int)((mousePos.x - boardOffset.x) / cellSize);
        int row = (int)((mousePos.y - boardOffset.y) / cellSize);

        if (row >= 0 && row < 3 && col >= 0 && col < 3 && board[row][col] == EMPTY) {
            // Blinking pixel cursor box (NES style)
            if (((int)(GetTime() * 6.0)) % 2 == 0) {
                Rectangle cursorRect = {
                    boardOffset.x + col * cellSize + 18,
                    boardOffset.y + row * cellSize + 18,
                    cellSize - 36,
                    cellSize - 36
                };
                DrawRectangleLinesEx(cursorRect, 4.0f, (turn == PLAYER_X) ? NES_RED : NES_CYAN);
            }
        }
    }

    void DrawPixelPieces() {
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                if (board[r][c] == EMPTY) continue;

                float eased = RetroEaseOutBack(anims[r][c].progress);
                // Snap drawn sprite size to integer multiples of 16px so pixels never distort
                int currentScale = std::max(1, (int)roundf((pixelScale - 1) * eased));
                float drawSize = 16.0f * currentScale;

                Vector2 center = {
                    boardOffset.x + c * cellSize + cellSize / 2.0f,
                    boardOffset.y + r * cellSize + cellSize / 2.0f
                };

                Texture2D activeTex = (board[r][c] == PLAYER_X) ? texX : texO;

                Rectangle src = { 0.0f, 0.0f, 16.0f, 16.0f };
                Rectangle dst = {
                    floorf(center.x - drawSize / 2.0f),
                    floorf(center.y - drawSize / 2.0f),
                    drawSize,
                    drawSize
                };

                DrawTexturePro(activeTex, src, dst, (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);
            }
        }
    }

    void DrawPixelWinLine() {
        if (winType == WIN_NONE || winLineAnimProgress <= 0.0f) return;

        // Quantize win line sweep into 16 discrete retro steps
        float steppedProgress = floorf(winLineAnimProgress * 16.0f) / 16.0f;
        if (winLineAnimProgress >= 0.99f) steppedProgress = 1.0f;

        Vector2 start = { 0 }, end = { 0 };
        if (winType == WIN_ROW) {
            start = { boardOffset.x + 18, boardOffset.y + winIndex * cellSize + cellSize / 2.0f };
            end   = { boardOffset.x + boardSize - 18, start.y };
        } else if (winType == WIN_COL) {
            start = { boardOffset.x + winIndex * cellSize + cellSize / 2.0f, boardOffset.y + 18 };
            end   = { start.x, boardOffset.y + boardSize - 18 };
        } else if (winType == WIN_DIAG_MAIN) {
            start = { boardOffset.x + 18, boardOffset.y + 18 };
            end   = { boardOffset.x + boardSize - 18, boardOffset.y + boardSize - 18 };
        } else if (winType == WIN_DIAG_ANTI) {
            start = { boardOffset.x + boardSize - 18, boardOffset.y + 18 };
            end   = { boardOffset.x + 18, boardOffset.y + boardSize - 18 };
        }

        Vector2 currentEnd = Vector2Lerp(start, end, steppedProgress);

        // Draw chunky pixelated blocks along the line vector
        int steps = 32;
        for (int i = 0; i <= steps; i++) {
            float t = (float)i / steps;
            Vector2 pt = Vector2Lerp(start, currentEnd, t);
            // Snap each pixel block to the 9px pixel grid
            int snapX = ((int)pt.x / pixelScale) * pixelScale;
            int snapY = ((int)pt.y / pixelScale) * pixelScale;
            DrawRectangle(snapX, snapY, pixelScale * 2, pixelScale * 2, NES_GREEN);
        }
    }

    void DrawFooterControls() {
        const char* footer = soundEnabled ? "[R] RESTART   |   [M] SFX: ON" : "[R] RESTART   |   [M] SFX: OFF";
        DrawText(footer, 300 - MeasureText(footer, 16) / 2, 645, 16, NES_LIGHT_GRAY);
    }
};

int main() {
    InitWindow(600, 700, "NES Retro Tic-Tac-Toe");
    InitAudioDevice();
    SetTargetFPS(60);

    {
        RetroTicTacToe game;

        while (!WindowShouldClose()) {
            float dt = GetFrameTime();
            game.Update(dt);

            BeginDrawing();
                ClearBackground(NES_BLACK);
                game.Draw();
            EndDrawing();
        }
    } // Ensures textures/sounds unload before CloseAudioDevice/CloseWindow

    CloseAudioDevice();
    CloseWindow();
    return 0;
}