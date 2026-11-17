#include <GL/freeglut.h>
#include <GL/gl.h>
#include <cmath>
#include <algorithm>
#include <cstdio>

// ============================================================================
// MODERN + NES HYBRID COLOR PALETTE
// ============================================================================
struct ColorRGBA {
    float r, g, b, a;
};

const ColorRGBA BG_DARK        = { 15.0f / 255.0f,  17.0f / 255.0f,  23.0f / 255.0f, 1.0f };
const ColorRGBA CARD_BG        = { 24.0f / 255.0f,  28.0f / 255.0f,  38.0f / 255.0f, 1.0f };
const ColorRGBA TILE_BG        = { 33.0f / 255.0f,  38.0f / 255.0f,  52.0f / 255.0f, 1.0f };
const ColorRGBA TILE_HOVER     = { 45.0f / 255.0f,  52.0f / 255.0f,  70.0f / 255.0f, 1.0f };
const ColorRGBA BORDER_SUBTLE  = { 58.0f / 255.0f,  66.0f / 255.0f,  86.0f / 255.0f, 1.0f };
const ColorRGBA SHADOW_COLOR   = { 0.0f,            0.0f,            0.0f,           0.35f };

const ColorRGBA TEXT_WHITE     = { 248.0f / 255.0f, 250.0f / 255.0f, 252.0f / 255.0f, 1.0f };
const ColorRGBA TEXT_MUTED     = { 148.0f / 255.0f, 163.0f / 255.0f, 184.0f / 255.0f, 1.0f };

// NES Accent Colors
const ColorRGBA ACCENT_X       = { 252.0f / 255.0f, 92.0f / 255.0f,  76.0f / 255.0f,  1.0f };
const ColorRGBA ACCENT_X_GLOW  = { 216.0f / 255.0f, 40.0f / 255.0f,  0.0f / 255.0f,   0.22f };
const ColorRGBA ACCENT_O       = { 56.0f / 255.0f,  189.0f / 255.0f, 248.0f / 255.0f, 1.0f };
const ColorRGBA ACCENT_O_GLOW  = { 0.0f / 255.0f,   112.0f / 255.0f, 236.0f / 255.0f, 0.22f };
const ColorRGBA ACCENT_WIN     = { 34.0f / 255.0f,  197.0f / 255.0f, 94.0f / 255.0f,  1.0f };
const ColorRGBA ACCENT_DRAW    = { 250.0f / 255.0f, 204.0f / 255.0f, 21.0f / 255.0f,  1.0f };

void setColor(const ColorRGBA& c, float alphaOverride = -1.0f) {
    glColor4f(c.r, c.g, c.b, (alphaOverride >= 0.0f) ? alphaOverride : c.a);
}

// ============================================================================
// GAME STATE & CONSTANTS
// ============================================================================
enum CellState { EMPTY = 0, PLAYER_X = 1, PLAYER_O = 2 };
enum WinType { WIN_NONE = 0, WIN_ROW, WIN_COL, WIN_DIAG_MAIN, WIN_DIAG_ANTI };

const int WIN_W = 600;
const int WIN_H = 740;

const float BOARD_X = 84.0f;
const float BOARD_Y = 175.0f;
const float CELL_SIZE = 144.0f;
const float BOARD_SIZE = CELL_SIZE * 3.0f; // 432px

CellState board[3][3] = {};
float animProgress[3][3] = {};
float hoverAlpha[3][3] = {};

int turn = PLAYER_X;
int winner = 0; // 0: Ongoing, 1: X, 2: O, 3: Draw
WinType winType = WIN_NONE;
int winIndex = -1;

int scoreX = 0;
int scoreO = 0;
int scoreDraw = 0;

int hoverRow = -1;
int hoverCol = -1;

float gridAnimProgress = 0.0f;
float winLineProgress = 0.0f;
float globalTime = 0.0f;

GLuint texX = 0;
GLuint texO = 0;

// ============================================================================
// MATH & EASING HELPERS
// ============================================================================
float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

float clamp01(float v) {
    return std::max(0.0f, std::min(1.0f, v));
}

// Smooth springy pop-in with slight retro step quantization
float EaseOutBack(float x) {
    x = clamp01(x);
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    return 1.0f + c3 * std::pow(x - 1.0f, 3.0f) + c1 * std::pow(x - 1.0f, 2.0f);
}

// ============================================================================
// PROCEDURAL 16x16 PIXEL ART OPENGL TEXTURE GENERATOR
// ============================================================================
GLuint CreatePixelTexture(const char* map[16], unsigned char p1[3], unsigned char p2[3]) {
    unsigned char pixels[16 * 16 * 4];

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            int idx = (y * 16 + x) * 4;
            char ch = map[y][x];
            if (ch == '1') {
                pixels[idx + 0] = p1[0];
                pixels[idx + 1] = p1[1];
                pixels[idx + 2] = p1[2];
                pixels[idx + 3] = 255;
            } else if (ch == '2') {
                pixels[idx + 0] = p2[0];
                pixels[idx + 1] = p2[1];
                pixels[idx + 2] = p2[2];
                pixels[idx + 3] = 255;
            } else {
                pixels[idx + 0] = 0;
                pixels[idx + 1] = 0;
                pixels[idx + 2] = 0;
                pixels[idx + 3] = 0;
            }
        }
    }

    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);

    return texID;
}

void InitSprites() {
    const char* mapX[16] = {
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

    const char* mapO[16] = {
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

    unsigned char redPrimary[3]   = { 216, 40, 0 };
    unsigned char redHighlight[3] = { 252, 116, 96 };
    unsigned char bluePrimary[3]  = { 0, 112, 236 };
    unsigned char cyanHighlight[3]= { 60, 188, 252 };

    texX = CreatePixelTexture(mapX, redPrimary, redHighlight);
    texO = CreatePixelTexture(mapO, bluePrimary, cyanHighlight);
}

// ============================================================================
// MODERN 2D PRIMITIVE RENDERERS
// ============================================================================
void DrawRoundedRect(float x, float y, float w, float h, float radius, const ColorRGBA& col) {
    setColor(col);
    const int segments = 10;
    float r = std::min(radius, std::min(w * 0.5f, h * 0.5f));

    glBegin(GL_POLYGON);
    // Top-right corner
    for (int i = 0; i <= segments; i++) {
        float a = (3.1415926f * 1.5f) + (3.1415926f * 0.5f) * ((float)i / segments);
        glVertex2f(x + w - r + std::cos(a) * r, y + r + std::sin(a) * r);
    }
    // Bottom-right corner
    for (int i = 0; i <= segments; i++) {
        float a = 0.0f + (3.1415926f * 0.5f) * ((float)i / segments);
        glVertex2f(x + w - r + std::cos(a) * r, y + h - r + std::sin(a) * r);
    }
    // Bottom-left corner
    for (int i = 0; i <= segments; i++) {
        float a = (3.1415926f * 0.5f) + (3.1415926f * 0.5f) * ((float)i / segments);
        glVertex2f(x + r + std::cos(a) * r, y + h - r + std::sin(a) * r);
    }
    // Top-left corner
    for (int i = 0; i <= segments; i++) {
        float a = 3.1415926f + (3.1415926f * 0.5f) * ((float)i / segments);
        glVertex2f(x + r + std::cos(a) * r, y + r + std::sin(a) * r);
    }
    glEnd();
}

void DrawRoundedOutline(float x, float y, float w, float h, float radius, float lineWidth, const ColorRGBA& col) {
    setColor(col);
    glLineWidth(lineWidth);
    const int segments = 10;
    float r = std::min(radius, std::min(w * 0.5f, h * 0.5f));

    glBegin(GL_LINE_LOOP);
    for (int i = 0; i <= segments; i++) {
        float a = (3.1415926f * 1.5f) + (3.1415926f * 0.5f) * ((float)i / segments);
        glVertex2f(x + w - r + std::cos(a) * r, y + r + std::sin(a) * r);
    }
    for (int i = 0; i <= segments; i++) {
        float a = 0.0f + (3.1415926f * 0.5f) * ((float)i / segments);
        glVertex2f(x + w - r + std::cos(a) * r, y + h - r + std::sin(a) * r);
    }
    for (int i = 0; i <= segments; i++) {
        float a = (3.1415926f * 0.5f) + (3.1415926f * 0.5f) * ((float)i / segments);
        glVertex2f(x + r + std::cos(a) * r, y + h - r + std::sin(a) * r);
    }
    for (int i = 0; i <= segments; i++) {
        float a = 3.1415926f + (3.1415926f * 0.5f) * ((float)i / segments);
        glVertex2f(x + r + std::cos(a) * r, y + r + std::sin(a) * r);
    }
    glEnd();
    glLineWidth(1.0f);
}

void DrawCapsuleSegment(float x1, float y1, float x2, float y2, float thickness, const ColorRGBA& col) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f) return;

    float r = thickness * 0.5f;
    float nx = (-dy / len) * r;
    float ny = (dx / len) * r;

    setColor(col);
    glBegin(GL_QUADS);
        glVertex2f(x1 + nx, y1 + ny);
        glVertex2f(x2 + nx, y2 + ny);
        glVertex2f(x2 - nx, y2 - ny);
        glVertex2f(x1 - nx, y1 - ny);
    glEnd();

    // Rounded end caps
    const int capSegs = 12;
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x1, y1);
    for (int i = 0; i <= capSegs; i++) {
        float a = 2.0f * 3.1415926f * i / capSegs;
        glVertex2f(x1 + std::cos(a) * r, y1 + std::sin(a) * r);
    }
    glEnd();

    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x2, y2);
    for (int i = 0; i <= capSegs; i++) {
        float a = 2.0f * 3.1415926f * i / capSegs;
        glVertex2f(x2 + std::cos(a) * r, y2 + std::sin(a) * r);
    }
    glEnd();
}

int GetBitmapTextWidth(const char* text, void* font = GLUT_BITMAP_HELVETICA_18) {
    int width = 0;
    for (const char* c = text; *c != '\0'; c++) {
        width += glutBitmapWidth(font, *c);
    }
    return width;
}

void DrawCenteredText(const char* text, float centerX, float y, void* font, const ColorRGBA& col) {
    int w = GetBitmapTextWidth(text, font);
    setColor(col);
    glRasterPos2f(centerX - w * 0.5f, y);
    for (const char* c = text; *c != '\0'; c++) {
        glutBitmapCharacter(font, *c);
    }
}

// ============================================================================
// GAME LOGIC
// ============================================================================
void ResetGame(bool clearScores = false) {
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            board[r][c] = EMPTY;
            animProgress[r][c] = 0.0f;
            hoverAlpha[r][c] = 0.0f;
        }
    }
    turn = PLAYER_X;
    winner = 0;
    winType = WIN_NONE;
    winIndex = -1;
    gridAnimProgress = 0.0f;
    winLineProgress = 0.0f;

    if (clearScores) {
        scoreX = 0;
        scoreO = 0;
        scoreDraw = 0;
    }
}

bool IsWinningCell(int r, int c) {
    if (winType == WIN_ROW) return r == winIndex;
    if (winType == WIN_COL) return c == winIndex;
    if (winType == WIN_DIAG_MAIN) return r == c;
    if (winType == WIN_DIAG_ANTI) return (r + c) == 2;
    return false;
}

void CheckWinner() {
    for (int i = 0; i < 3; i++) {
        if (board[i][0] != EMPTY && board[i][0] == board[i][1] && board[i][1] == board[i][2]) {
            winner = board[i][0];
            winType = WIN_ROW;
            winIndex = i;
            if (winner == PLAYER_X) scoreX++; else scoreO++;
            return;
        }
        if (board[0][i] != EMPTY && board[0][i] == board[1][i] && board[1][i] == board[2][i]) {
            winner = board[0][i];
            winType = WIN_COL;
            winIndex = i;
            if (winner == PLAYER_X) scoreX++; else scoreO++;
            return;
        }
    }

    if (board[0][0] != EMPTY && board[0][0] == board[1][1] && board[1][1] == board[2][2]) {
        winner = board[0][0];
        winType = WIN_DIAG_MAIN;
        if (winner == PLAYER_X) scoreX++; else scoreO++;
        return;
    }
    if (board[0][2] != EMPTY && board[0][2] == board[1][1] && board[1][1] == board[2][0]) {
        winner = board[0][2];
        winType = WIN_DIAG_ANTI;
        if (winner == PLAYER_X) scoreX++; else scoreO++;
        return;
    }

    bool emptyLeft = false;
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            if (board[r][c] == EMPTY) emptyLeft = true;
        }
    }
    if (!emptyLeft) {
        winner = 3;
        scoreDraw++;
    }
}

// ============================================================================
// UI & BOARD RENDERING
// ============================================================================
void DrawHeaderAndScoreboard() {
    // Title
    DrawCenteredText("TIC-TAC-TOE // 8-BIT EDITION", WIN_W * 0.5f, 42.0f, GLUT_BITMAP_HELVETICA_18, TEXT_WHITE);

    // Three modern score cards (Player X, Draws, Player O)
    float cardW = 132.0f;
    float cardH = 64.0f;
    float gap = 18.0f;
    float totalW = cardW * 3.0f + gap * 2.0f;
    float startX = (WIN_W - totalW) * 0.5f;
    float cardY = 66.0f;

    char buf[32];

    // --- Card 1: 1UP [X] ---
    bool activeX = (winner == 0 && turn == PLAYER_X) || (winner == PLAYER_X);
    DrawRoundedRect(startX, cardY + 3.0f, cardW, cardH, 12.0f, SHADOW_COLOR);
    DrawRoundedRect(startX, cardY, cardW, cardH, 12.0f, activeX ? TILE_HOVER : CARD_BG);
    DrawRoundedOutline(startX, cardY, cardW, cardH, 12.0f, activeX ? 2.0f : 1.0f, activeX ? ACCENT_X : BORDER_SUBTLE);
    DrawCenteredText("1UP [X]", startX + cardW * 0.5f, cardY + 24.0f, GLUT_BITMAP_9_BY_15, ACCENT_X);
    std::snprintf(buf, sizeof(buf), "%d", scoreX);
    DrawCenteredText(buf, startX + cardW * 0.5f, cardY + 48.0f, GLUT_BITMAP_HELVETICA_18, TEXT_WHITE);

    // --- Card 2: DRAWS ---
    float c2X = startX + cardW + gap;
    bool activeDraw = (winner == 3);
    DrawRoundedRect(c2X, cardY + 3.0f, cardW, cardH, 12.0f, SHADOW_COLOR);
    DrawRoundedRect(c2X, cardY, cardW, cardH, 12.0f, activeDraw ? TILE_HOVER : CARD_BG);
    DrawRoundedOutline(c2X, cardY, cardW, cardH, 12.0f, activeDraw ? 2.0f : 1.0f, activeDraw ? ACCENT_DRAW : BORDER_SUBTLE);
    DrawCenteredText("DRAWS", c2X + cardW * 0.5f, cardY + 24.0f, GLUT_BITMAP_9_BY_15, TEXT_MUTED);
    std::snprintf(buf, sizeof(buf), "%d", scoreDraw);
    DrawCenteredText(buf, c2X + cardW * 0.5f, cardY + 48.0f, GLUT_BITMAP_HELVETICA_18, TEXT_WHITE);

    // --- Card 3: 2UP [O] ---
    float c3X = startX + (cardW + gap) * 2.0f;
    bool activeO = (winner == 0 && turn == PLAYER_O) || (winner == PLAYER_O);
    DrawRoundedRect(c3X, cardY + 3.0f, cardW, cardH, 12.0f, SHADOW_COLOR);
    DrawRoundedRect(c3X, cardY, cardW, cardH, 12.0f, activeO ? TILE_HOVER : CARD_BG);
    DrawRoundedOutline(c3X, cardY, cardW, cardH, 12.0f, activeO ? 2.0f : 1.0f, activeO ? ACCENT_O : BORDER_SUBTLE);
    DrawCenteredText("2UP [O]", c3X + cardW * 0.5f, cardY + 24.0f, GLUT_BITMAP_9_BY_15, ACCENT_O);
    std::snprintf(buf, sizeof(buf), "%d", scoreO);
    DrawCenteredText(buf, c3X + cardW * 0.5f, cardY + 48.0f, GLUT_BITMAP_HELVETICA_18, TEXT_WHITE);

    // Status Banner Pill
    const char* statusText = "";
    ColorRGBA statusColor = TEXT_WHITE;
    if (winner == PLAYER_X) {
        statusText = "1UP [X] WINS THE ROUND!";
        statusColor = ACCENT_X;
    } else if (winner == PLAYER_O) {
        statusText = "2UP [O] WINS THE ROUND!";
        statusColor = ACCENT_O;
    } else if (winner == 3) {
        statusText = "ROUND ENDED IN A DRAW";
        statusColor = ACCENT_DRAW;
    } else {
        statusText = (turn == PLAYER_X) ? "1UP [X] TO MOVE" : "2UP [O] TO MOVE";
        statusColor = (turn == PLAYER_X) ? ACCENT_X : ACCENT_O;
    }
    DrawCenteredText(statusText, WIN_W * 0.5f, 156.0f, GLUT_BITMAP_9_BY_15, statusColor);
}

void DrawBoardContainerAndTiles() {
    float pad = 16.0f;
    float scale = EaseOutBack(gridAnimProgress);
    float cx = BOARD_X + BOARD_SIZE * 0.5f;
    float cy = BOARD_Y + BOARD_SIZE * 0.5f;

    glPushMatrix();
    glTranslatef(cx, cy, 0.0f);
    glScalef(scale, scale, 1.0f);
    glTranslatef(-cx, -cy, 0.0f);

    // Outer Board Card + Drop Shadow
    DrawRoundedRect(BOARD_X - pad, BOARD_Y - pad + 8.0f, BOARD_SIZE + pad * 2.0f, BOARD_SIZE + pad * 2.0f, 20.0f, SHADOW_COLOR);
    DrawRoundedRect(BOARD_X - pad, BOARD_Y - pad, BOARD_SIZE + pad * 2.0f, BOARD_SIZE + pad * 2.0f, 20.0f, CARD_BG);
    DrawRoundedOutline(BOARD_X - pad, BOARD_Y - pad, BOARD_SIZE + pad * 2.0f, BOARD_SIZE + pad * 2.0f, 20.0f, 1.5f, BORDER_SUBTLE);

    // Individual Recessed Cell Tiles
    float tileGap = 8.0f;
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            float tx = BOARD_X + c * CELL_SIZE + tileGap;
            float ty = BOARD_Y + r * CELL_SIZE + tileGap;
            float tw = CELL_SIZE - tileGap * 2.0f;
            float th = CELL_SIZE - tileGap * 2.0f;

            // Base tile
            DrawRoundedRect(tx, ty, tw, th, 14.0f, TILE_BG);

            // Smooth hover tint & outline
            if (hoverAlpha[r][c] > 0.01f && board[r][c] == EMPTY && winner == 0) {
                ColorRGBA glow = (turn == PLAYER_X) ? ACCENT_X_GLOW : ACCENT_O_GLOW;
                glow.a *= hoverAlpha[r][c];
                DrawRoundedRect(tx, ty, tw, th, 14.0f, glow);

                ColorRGBA outlineCol = (turn == PLAYER_X) ? ACCENT_X : ACCENT_O;
                outlineCol.a = 0.65f * hoverAlpha[r][c];
                DrawRoundedOutline(tx, ty, tw, th, 14.0f, 2.0f, outlineCol);
            }

            // Highlight winning cells with a subtle green aura
            if (winner != 0 && winner != 3 && IsWinningCell(r, c)) {
                float pulse = 0.25f + 0.10f * std::sin(globalTime * 6.0f);
                ColorRGBA winGlow = ACCENT_WIN;
                winGlow.a = pulse;
                DrawRoundedRect(tx, ty, tw, th, 14.0f, winGlow);
                DrawRoundedOutline(tx, ty, tw, th, 14.0f, 2.0f, ACCENT_WIN);
            }
        }
    }

    glPopMatrix();
}

void DrawSprites() {
    glEnable(GL_TEXTURE_2D);

    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            float cx = BOARD_X + c * CELL_SIZE + CELL_SIZE * 0.5f;
            float cy = BOARD_Y + r * CELL_SIZE + CELL_SIZE * 0.5f;

            // Ghost preview on hover
            if (board[r][c] == EMPTY && winner == 0 && hoverAlpha[r][c] > 0.01f) {
                GLuint ghostTex = (turn == PLAYER_X) ? texX : texO;
                glBindTexture(GL_TEXTURE_2D, ghostTex);
                glColor4f(1.0f, 1.0f, 1.0f, 0.22f * hoverAlpha[r][c]);

                float half = 48.0f; // 16px * 6
                glBegin(GL_QUADS);
                    glTexCoord2f(0.0f, 0.0f); glVertex2f(cx - half, cy - half);
                    glTexCoord2f(1.0f, 0.0f); glVertex2f(cx + half, cy - half);
                    glTexCoord2f(1.0f, 1.0f); glVertex2f(cx + half, cy + half);
                    glTexCoord2f(0.0f, 1.0f); glVertex2f(cx - half, cy + half);
                glEnd();
            }

            // Placed pieces with springy pop-in
            if (board[r][c] != EMPTY) {
                float eased = EaseOutBack(animProgress[r][c]);
                float size = 96.0f * eased; // 16px * 6 = 96px target size
                float half = size * 0.5f;

                GLuint activeTex = (board[r][c] == PLAYER_X) ? texX : texO;
                glBindTexture(GL_TEXTURE_2D, activeTex);

                // Subtle drop shadow under pixel sprite
                glColor4f(0.0f, 0.0f, 0.0f, 0.40f);
                glBegin(GL_QUADS);
                    glTexCoord2f(0.0f, 0.0f); glVertex2f(cx - half + 4.0f, cy - half + 6.0f);
                    glTexCoord2f(1.0f, 0.0f); glVertex2f(cx + half + 4.0f, cy - half + 6.0f);
                    glTexCoord2f(1.0f, 1.0f); glVertex2f(cx + half + 4.0f, cy + half + 6.0f);
                    glTexCoord2f(0.0f, 1.0f); glVertex2f(cx - half + 4.0f, cy + half + 6.0f);
                glEnd();

                // Crisp pixel sprite
                glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
                glBegin(GL_QUADS);
                    glTexCoord2f(0.0f, 0.0f); glVertex2f(cx - half, cy - half);
                    glTexCoord2f(1.0f, 0.0f); glVertex2f(cx + half, cy - half);
                    glTexCoord2f(1.0f, 1.0f); glVertex2f(cx + half, cy + half);
                    glTexCoord2f(0.0f, 1.0f); glVertex2f(cx - half, cy + half);
                glEnd();
            }
        }
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
}

void DrawWinStrikeLine() {
    if (winType == WIN_NONE || winLineProgress <= 0.0f) return;

    float t = clamp01(winLineProgress);
    // Smooth ease-out cubic
    float eased = 1.0f - std::pow(1.0f - t, 3.0f);

    float x1 = 0.0f, y1 = 0.0f, x2 = 0.0f, y2 = 0.0f;
    float margin = 28.0f;

    if (winType == WIN_ROW) {
        x1 = BOARD_X + margin;
        x2 = BOARD_X + BOARD_SIZE - margin;
        y1 = y2 = BOARD_Y + winIndex * CELL_SIZE + CELL_SIZE * 0.5f;
    } else if (winType == WIN_COL) {
        x1 = x2 = BOARD_X + winIndex * CELL_SIZE + CELL_SIZE * 0.5f;
        y1 = BOARD_Y + margin;
        y2 = BOARD_Y + BOARD_SIZE - margin;
    } else if (winType == WIN_DIAG_MAIN) {
        x1 = BOARD_X + margin;
        y1 = BOARD_Y + margin;
        x2 = BOARD_X + BOARD_SIZE - margin;
        y2 = BOARD_Y + BOARD_SIZE - margin;
    } else if (winType == WIN_DIAG_ANTI) {
        x1 = BOARD_X + BOARD_SIZE - margin;
        y1 = BOARD_Y + margin;
        x2 = BOARD_X + margin;
        y2 = BOARD_Y + BOARD_SIZE - margin;
    }

    float curX = lerp(x1, x2, eased);
    float curY = lerp(y1, y2, eased);

    // Outer glow + solid core capsule
    ColorRGBA glow = ACCENT_WIN;
    glow.a = 0.35f;
    DrawCapsuleSegment(x1, y1, curX, curY, 22.0f, glow);
    DrawCapsuleSegment(x1, y1, curX, curY, 10.0f, ACCENT_WIN);
    DrawCapsuleSegment(x1, y1, curX, curY, 4.0f, TEXT_WHITE);
}

void DrawFooter() {
    DrawCenteredText("[R] NEW ROUND    |    [C] RESET SCORES    |    [ESC] QUIT", WIN_W * 0.5f, 675.0f, GLUT_BITMAP_9_BY_15, TEXT_MUTED);
}

// ============================================================================
// GLUT CALLBACKS
// ============================================================================
void Display() {
    glClear(GL_COLOR_BUFFER_BIT);

    DrawHeaderAndScoreboard();
    DrawBoardContainerAndTiles();
    DrawSprites();
    DrawWinStrikeLine();
    DrawFooter();

    glutSwapBuffers();
}

void Update(int value) {
    const float dt = 0.016f;
    globalTime += dt;

    if (gridAnimProgress < 1.0f) {
        gridAnimProgress = clamp01(gridAnimProgress + dt * 3.2f);
    }

    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            if (board[r][c] != EMPTY && animProgress[r][c] < 1.0f) {
                animProgress[r][c] = clamp01(animProgress[r][c] + dt * 4.5f);
            }

            float targetHover = (winner == 0 && board[r][c] == EMPTY && r == hoverRow && c == hoverCol) ? 1.0f : 0.0f;
            hoverAlpha[r][c] = lerp(hoverAlpha[r][c], targetHover, dt * 14.0f);
        }
    }

    if (winner == PLAYER_X || winner == PLAYER_O) {
        if (winLineProgress < 1.0f) {
            winLineProgress = clamp01(winLineProgress + dt * 3.8f);
        }
    }

    glutPostRedisplay();
    glutTimerFunc(16, Update, 0);
}

void PassiveMotion(int x, int y) {
    int winW = glutGet(GLUT_WINDOW_WIDTH);
    int winH = glutGet(GLUT_WINDOW_HEIGHT);

    float vx = (float)x / winW * WIN_W;
    float vy = (float)y / winH * WIN_H;

    int c = (int)std::floor((vx - BOARD_X) / CELL_SIZE);
    int r = (int)std::floor((vy - BOARD_Y) / CELL_SIZE);

    if (r >= 0 && r < 3 && c >= 0 && c < 3) {
        hoverRow = r;
        hoverCol = c;
    } else {
        hoverRow = -1;
        hoverCol = -1;
    }
}

void Mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN && winner == 0 && gridAnimProgress >= 0.85f) {
        PassiveMotion(x, y);
        if (hoverRow >= 0 && hoverRow < 3 && hoverCol >= 0 && hoverCol < 3) {
            if (board[hoverRow][hoverCol] == EMPTY) {
                board[hoverRow][hoverCol] = (CellState)turn;
                animProgress[hoverRow][hoverCol] = 0.0f;

                CheckWinner();
                if (winner == 0) {
                    turn = (turn == PLAYER_X) ? PLAYER_O : PLAYER_X;
                }
            }
        }
    }
}

void Keyboard(unsigned char key, int x, int y) {
    if (key == 'r' || key == 'R') {
        ResetGame(false);
    } else if (key == 'c' || key == 'C') {
        ResetGame(true);
    } else if (key == 27) { // ESC
        exit(0);
    }
}

void InitOpenGL() {
    glClearColor(BG_DARK.r, BG_DARK.g, BG_DARK.b, 1.0f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    // Top-left origin (0,0) matching Raylib screen coordinates
    gluOrtho2D(0.0, WIN_W, WIN_H, 0.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    InitSprites();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_MULTISAMPLE);
    glutInitWindowSize(WIN_W, WIN_H);
    glutInitWindowPosition(120, 80);
    glutCreateWindow("Tic-Tac-Toe // Modern 8-Bit OpenGL");

    InitOpenGL();

    glutDisplayFunc(Display);
    glutPassiveMotionFunc(PassiveMotion);
    glutMouseFunc(Mouse);
    glutKeyboardFunc(Keyboard);
    glutTimerFunc(0, Update, 0);

    glutMainLoop();
    return 0;
}