// =======================================================
// Part 1: File Header, Build Command, Headers, Macros
// Details: All headers, macros, and library links are in this part.
// =======================================================

// dxball_simple.cpp
// A simplified DX-Ball clone with a text-based menu.
// Uses FreeGLUT + OpenGL. No external image files are required.
// Build (MinGW): g++ dxball_simple.cpp -o dxball_simple.exe -lfreeglut -lopengl32 -lglu32 -lwinmm -std=c++11 -mconsole

#define _USE_MATH_DEFINES
#include <GL/glut.h>
#include <cmath>
#include <vector>
#include <string>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

// =======================================================
// Part 2: Window & Global Constants
// Details: Window size, score file names, etc.
// =======================================================

const int WIN_W = 800;
const int WIN_H = 600;
const char* SCORE_FILE = "scores.txt";
const int MAX_RECENT = 5;

// =======================================================
// Part 3: Game Types, Globals & Gameplay State
// Details: Game state enum, entities (Ball/Paddle/Brick/Perk) & globals.
// =======================================================

enum GameState { GS_MENU, GS_PLAYING, GS_PAUSED, GS_LEVEL_CLEAR, GS_GAMEOVER, GS_HELP, GS_SCOREBOARD, GS_MUSIC_MENU };

GameState gameState = GS_MENU;
int currentLevel = 1;

// Entities
struct Ball {
    float x,y,vx,vy,radius,speed;
    bool stuck;
    bool isFireball;
    float fireballTimer;
} ball;
struct Paddle { float x,y,w,h,speed; } paddle;
struct Brick { float x,y,w,h; int hits; bool alive; int type; };
struct Perk { float x,y,vy; int type; bool alive; };
struct Projectile { float x, y, vy; bool alive; };

std::vector<Brick> bricks;
std::vector<Perk> perks;
std::vector<Projectile> projectiles;

// Gameplay state
int score = 0;
int lives = 3;
int highScore = 0;
int bricksRemaining = 0;
double gameStartTime = 0.0;
double elapsedTime = 0.0;
float fireCooldown = 0.0f;
const float FIRE_RATE = 0.3f;

const float PERK_DROP_PROB = 0.25f;
const float BALL_SPEED_MAX = 900.0f;
const float BALL_SPEED_INCREASE_RATE = 5.0f;

// Input flags
bool keyLeft=false, keyRight=false;
bool musicPlaying=false;

// =======================================================
// Part 4: Forward Declarations
// Details: Prototypes for functions defined later.
// =======================================================

void drawText(float x, float y, const std::string &s);
void drawRect(float x, float y, float w, float h);
void resetPaddleAndBall();
void createBricksForLevel(int level);
void startNewGame();
void startLevel(int level);
void openHelpFile();
void playMusic();
void stopMusic();
int loadHighScore();
void saveHighScore(int newScore);
void saveScore(int s);
std::vector<int> loadRecentScores();

// =======================================================
// Part 5: Utility Drawing Helpers
// Details: Functions to draw text and rectangles.
// =======================================================

void drawText(float x, float y, const std::string &s) {
    glRasterPos2f(x,y);
    for(char c : s) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
}

void drawRect(float x, float y, float w, float h) {
    glBegin(GL_QUADS);
      glVertex2f(x,y);
      glVertex2f(x+w,y);
      glVertex2f(x+w,y+h);
      glVertex2f(x,y+h);
    glEnd();
}

// =======================================================
// Part 6: Game Initialization & Level Creation
// Details: Reset paddle/ball, generate bricks, start game/level.
// =======================================================

void resetPaddleAndBall() {
    paddle.w = 120.0f;
    paddle.h = 16.0f;
    paddle.x = (WIN_W - paddle.w) / 2.0f;
    paddle.y = 50.0f;
    paddle.speed = 600.0f;

    ball.radius = 8.0f;
    ball.x = paddle.x + paddle.w/2.0f;
    ball.y = paddle.y + paddle.h + ball.radius + 1.0f;
    ball.speed = 380.0f;
    ball.vx = 0.0f; ball.vy = 0.0f;
    ball.stuck = true;
    ball.isFireball = false;
    ball.fireballTimer = 0.0f;
}

void createBricksForLevel(int level) {
    bricks.clear();
    perks.clear();
    projectiles.clear();
    int rows = std::min(3 + level, 8);
    int cols = 10;
    float margin = 60.0f, gap = 6.0f;
    float brickW = (WIN_W - 2*margin - (cols-1)*gap) / cols;
    float brickH = 22.0f;
    float startY = WIN_H - 100.0f;
    bricksRemaining = 0;

    for (int r=0;r<rows;++r) {
        for (int c=0;c<cols;++c) {
            Brick b;
            b.w = brickW; b.h = brickH;
            b.x = margin + c * (brickW + gap);
            b.y = startY - r * (brickH + gap);
            b.hits = ((rand()%100) < (std::max(0, level - 1) * 15)) ? 2 : 1;
            b.alive = true;
            b.type = ((rand()/(RAND_MAX+1.0f)) < (PERK_DROP_PROB + 0.02f*(level-1))) ? 1 : 0;
            bricks.push_back(b);
            bricksRemaining++;
        }
    }
    ball.speed = 380.0f + (level - 1) * 30.0f;
    if (ball.speed > BALL_SPEED_MAX) ball.speed = BALL_SPEED_MAX;
}

void startNewGame() {
    currentLevel = 1;
    score = 0; lives = 3;
    createBricksForLevel(currentLevel);
    resetPaddleAndBall();
    gameStartTime = glutGet(GLUT_ELAPSED_TIME);
    elapsedTime = 0.0;
    gameState = GS_PLAYING;
}

void startLevel(int level) {
    createBricksForLevel(level);
    resetPaddleAndBall();
    gameStartTime = glutGet(GLUT_ELAPSED_TIME);
    elapsedTime = 0.0;
    gameState = GS_PLAYING;
}

// =======================================================
// Part 7: Perks (Spawn & Apply)
// Details: Spawning and applying effects of power-ups.
// =======================================================

void spawnPerk(float x,float y) {
    Perk p; p.x = x; p.y = y; p.vy = -150.0f; p.alive=true;
    int t = rand()%100;
    if (t < 35) p.type=0;         // Extra Life (35%)
    else if (t < 65) p.type=1;    // Wide Paddle (30%)
    else if (t < 80) p.type=2;    // Speed Ball (15%)
    else if (t < 90) p.type=3;    // Fireball (10%)
    else if (t < 97) p.type=4;    // Shrink Paddle (7%)
    else p.type=5;                // Instant Death (3%)
    perks.push_back(p);
}

void applyPerk(Perk &p) {
    if (p.type==0) lives++;
    else if (p.type==1) { paddle.w += 40.0f; if (paddle.w>280.0f) paddle.w=280.0f; }
    else if (p.type==2) { ball.speed *= 1.15f; if (ball.speed>BALL_SPEED_MAX) ball.speed=BALL_SPEED_MAX; }
    else if (p.type==3) { ball.isFireball = true; ball.fireballTimer = 10.0f; }
    else if (p.type==4) { paddle.w -= 30.0f; if (paddle.w<40.0f) paddle.w=40.0f; }
    else if (p.type==5) {
        lives--;
        if (lives <= 0) {
            saveScore(score);
            saveHighScore(score);
            gameState = GS_GAMEOVER;
        } else {
            resetPaddleAndBall();
        }
    }
    p.alive=false;
}

// =======================================================
// Part 8: Ball Physics & Collision Handling
// Details: Ball launch, wall/paddle/brick collision, etc.
// =======================================================

void launchBall() {
    if (!ball.stuck) return;
    ball.stuck = false;
    float angle = M_PI/3.0f + (rand()%100 - 50) * 0.004f;
    ball.vx = ball.speed * cosf(angle);
    ball.vy = ball.speed * sinf(angle);
}

void normalizeBallVelocity() {
    float vmag = sqrtf(ball.vx*ball.vx + ball.vy*ball.vy);
    if (vmag > 0.0001f) {
        ball.vx *= ball.speed / vmag;
        ball.vy *= ball.speed / vmag;
    }
}

void bounceBallOffPaddle() {
    float rel = (ball.x - (paddle.x + paddle.w*0.5f)) / (paddle.w*0.5f);
    float angle = (M_PI/2.0f) + rel * (75.0f * M_PI/180.0f);
    ball.y = paddle.y + paddle.h + ball.radius + 1.0f;
    ball.vx = ball.speed * cosf(angle);
    ball.vy = ball.speed * sinf(angle);
}

void handleWallCollisions() {
    if (ball.x - ball.radius <= 0) { ball.x = ball.radius; ball.vx = -ball.vx; }
    if (ball.x + ball.radius >= WIN_W) { ball.x = WIN_W - ball.radius; ball.vx = -ball.vx; }
    if (ball.y + ball.radius >= WIN_H) { ball.y = WIN_H - ball.radius; ball.vy = -ball.vy; }
}

void handlePaddleCollision() {
    if (ball.vy < 0 && ball.x+ball.radius > paddle.x && ball.x-ball.radius < paddle.x+paddle.w && ball.y-ball.radius < paddle.y+paddle.h && ball.y+ball.radius > paddle.y) {
        bounceBallOffPaddle();
    }
}

void handleBrickCollisions() {
    for (auto &b : bricks) {
        if (!b.alive) continue;
        if (ball.x+ball.radius > b.x && ball.x-ball.radius < b.x+b.w && ball.y+ball.radius > b.y && ball.y-ball.radius < b.y+b.h) {
            if (ball.isFireball) {
                b.alive = false; bricksRemaining--; score += 10;
                if (b.type==1) spawnPerk(b.x + b.w/2, b.y + b.h/2);
            } else {
                float overlapX = (b.w/2 + ball.radius) - fabs(ball.x - (b.x + b.w/2));
                float overlapY = (b.h/2 + ball.radius) - fabs(ball.y - (b.y + b.h/2));
                if (overlapX < overlapY) ball.vx = -ball.vx;
                else ball.vy = -ball.vy;

                b.hits--;
                if (b.hits <= 0) {
                    b.alive = false; bricksRemaining--; score += 10;
                    if (b.type==1) spawnPerk(b.x + b.w/2, b.y + b.h/2);
                } else score += 5;
                break;
            }
        }
    }
}

void handlePerks(double dt) {
    for (auto &p : perks) {
        if (!p.alive) continue;
        p.y += p.vy * dt;
        if (p.y < -40) p.alive=false;
        if (p.x > paddle.x && p.x < paddle.x+paddle.w && p.y < paddle.y+paddle.h && p.y > paddle.y) {
            applyPerk(p);
        }
    }
}

void handleProjectiles(double dt) {
    for (auto &p : projectiles) {
        if (!p.alive) continue;
        p.y += p.vy * dt;
        if (p.y > WIN_H) p.alive = false;

        for (auto &b : bricks) {
            if (b.alive && p.x>b.x && p.x<b.x+b.w && p.y>b.y && p.y<b.y+b.h) {
                p.alive = false;
                b.hits--;
                if (b.hits <= 0) {
                    b.alive = false; bricksRemaining--; score += 10;
                    if (b.type==1) spawnPerk(b.x+b.w/2, b.y+b.h/2);
                } else score += 5;
                break;
            }
        }
    }
}

// =======================================================
// Part 9: Game Update Loop
// Details: Movement, state changes, level progression.
// =======================================================

void increaseBallSpeedOverTime(double dt) {
    if (!ball.stuck) {
        ball.speed += BALL_SPEED_INCREASE_RATE * dt;
        if (ball.speed > BALL_SPEED_MAX) ball.speed = BALL_SPEED_MAX;
        normalizeBallVelocity();
    }
}

void updateGame(double dt) {
    if (gameState != GS_PLAYING) return;
    elapsedTime = (glutGet(GLUT_ELAPSED_TIME) - gameStartTime) / 1000.0;
    if (fireCooldown > 0) fireCooldown -= dt;

    if (ball.isFireball) {
        ball.fireballTimer -= dt;
        if (ball.fireballTimer <= 0) ball.isFireball = false;
    }

    float mv = paddle.speed * dt;
    if (keyLeft) paddle.x -= mv;
    if (keyRight) paddle.x += mv;
    if (paddle.x < 0) paddle.x = 0;
    if (paddle.x + paddle.w > WIN_W) paddle.x = WIN_W - paddle.w;

    if (ball.stuck) {
        ball.x = paddle.x + paddle.w/2.0f;
    } else {
        ball.x += ball.vx * dt;
        ball.y += ball.vy * dt;
    }

    handleWallCollisions();
    if (ball.y - ball.radius <= 0) {
        lives--;
        if (lives <= 0) {
            saveScore(score);
            saveHighScore(score);
            gameState = GS_GAMEOVER;
        } else {
            resetPaddleAndBall();
        }
        return;
    }
    handlePaddleCollision();
    handleBrickCollisions();
    handlePerks(dt);
    handleProjectiles(dt);
    increaseBallSpeedOverTime(dt);

    if (bricksRemaining <= 0) {
        saveScore(score);
        gameState = GS_LEVEL_CLEAR;
    }
}

// =======================================================
// Part 10: Score Persistence
// Details: Saving and loading recent scores and high score.
// =======================================================

void saveScore(int s) {
    std::vector<int> scores = loadRecentScores();
    scores.insert(scores.begin(), s);
    if ((int)scores.size() > MAX_RECENT) scores.resize(MAX_RECENT);
    std::ofstream ofs(SCORE_FILE, std::ios::trunc);
    if (ofs) for (int val : scores) ofs << val << "\n";
}

std::vector<int> loadRecentScores() {
    std::vector<int> v;
    std::ifstream ifs(SCORE_FILE);
    if (ifs) { int x; while (ifs >> x) v.push_back(x); }
    return v;
}

void saveHighScore(int newScore) {
    if (newScore > highScore) {
        highScore = newScore;
        std::ofstream ofs("highscore.txt");
        if (ofs) ofs << highScore;
    }
}

int loadHighScore() {
    int hs = 0;
    std::ifstream ifs("highscore.txt");
    if (ifs) ifs >> hs;
    return hs;
}

// =======================================================
// Part 11: Help File & Music Control
// Details: Opening help file and playing/stopping music.
// =======================================================

void openHelpFile() {
    std::ofstream ofs("help.txt", std::ios::app);
    if (ofs) {
        ofs.seekp(0, std::ios::end);
        if (ofs.tellp() == 0) {
            ofs << "DxBall Simple - Help\n\nControls:\n- Move paddle: Mouse or A/D or Left/Right arrows\n- Launch ball: Space\n- Shoot: Left Mouse Click\n- Pause: P\n\nPerks:\n- Extra life, Wider paddle, Speed up ball, Fireball\n- BEWARE: Shrink paddle, Instant Death\n";
        }
    }
    system("start notepad help.txt");
}

void playMusic() {
    stopMusic();
    if (GetFileAttributesA("music.wav") != INVALID_FILE_ATTRIBUTES) {
        PlaySoundA("music.wav", NULL, SND_ASYNC | SND_FILENAME | SND_LOOP);
        musicPlaying = true;
    } else {
        std::cerr << "music.wav not found\n";
    }
}

void stopMusic() {
    PlaySoundA(NULL, NULL, 0);
    musicPlaying = false;
}

// =======================================================
// Part 12: Rendering
// Details: All UI and scene rendering functions.
// =======================================================

void drawHUD() {
    std::ostringstream ss; glColor3f(1,1,1);
    ss << "Score: " << score; drawText(10, WIN_H - 24, ss.str());
    ss.str(""); ss.clear(); ss << "Lives: " << lives; drawText(10, WIN_H - 48, ss.str());
    ss.str(""); ss.clear(); ss << "Level: " << currentLevel; drawText(WIN_W - 120, WIN_H - 24, ss.str());
    ss.str(""); ss.clear(); ss << "Time: " << std::fixed << std::setprecision(1) << elapsedTime; drawText(WIN_W - 140, WIN_H - 48, ss.str());
}

void renderBricks() {
    for (auto &b : bricks) {
        if (!b.alive) continue;
        if (b.hits == 2) {
            glColor3f(0.75f, 0.75f, 0.75f); // Silver for tough bricks
        } else {
            glColor3f(0.2f, 0.5f, 1.0f); // Blue for normal bricks
        }
        drawRect(b.x, b.y, b.w, b.h);
    }
}

void renderPerks() {
    for (auto &p : perks) {
        if (!p.alive) continue;
        if (p.type==0) glColor3f(1.0f,0.8f,0.2f);       // Life
        else if (p.type==1) glColor3f(0.3f,0.8f,0.3f);  // Wide
        else if (p.type==2) glColor3f(1.0f,0.5f,0.3f);  // Speed
        else if (p.type==3) glColor3f(1.0f,0.1f,0.1f);  // Fireball
        else if (p.type==4) glColor3f(0.5f,0.2f,0.8f);  // Shrink
        else if (p.type==5) glColor3f(0.1f,0.1f,0.1f);  // Death
        
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(p.x,p.y);
        for (int i=0;i<=16;++i) {
            float a = (float)i/16 * 2.0f*M_PI;
            glVertex2f(p.x + cosf(a)*10.0f, p.y + sinf(a)*10.0f);
        }
        glEnd();
    }
}

void renderProjectiles() {
    glColor3f(1.0f, 1.0f, 0.2f);
    for (const auto &p : projectiles) {
        if (p.alive) drawRect(p.x - 2, p.y, 4, 12);
    }
}

void renderMenu() {
    glColor3f(1,1,1);
    drawText(WIN_W/2 - 100, WIN_H - 150, "DX-BALL SIMPLE");
    drawText(WIN_W/2 - 100, WIN_H - 220, "1. Play Game");
    drawText(WIN_W/2 - 100, WIN_H - 250, "2. High Scores");
    drawText(WIN_W/2 - 100, WIN_H - 280, "3. Music Options");
    drawText(WIN_W/2 - 100, WIN_H - 310, "4. Help");
    drawText(WIN_W/2 - 100, WIN_H - 340, "ESC. Exit");
}

void renderHelp() {
    glColor3f(1,1,1);
    drawText(60, WIN_H - 60, "HELP");
    drawText(60, WIN_H - 100, "- Move: Mouse or A/D or Left/Right");
    drawText(60, WIN_H - 130, "- Launch ball: Space");
    drawText(60, WIN_H - 160, "- Shoot: Left Mouse Click");
    drawText(60, WIN_H - 190, "- Pause: P");
    drawText(60, 40, "Press ESC to return");
}

void renderScoreboard() {
    glColor3f(1,1,1);
    drawText(WIN_W/2 - 90, WIN_H - 60, "High Score");
    std::ostringstream hs_ss; hs_ss << highScore;
    drawText(WIN_W/2 - 40, WIN_H - 90, hs_ss.str());

    drawText(WIN_W/2 - 90, WIN_H - 140, "Recent Scores");
    auto scores = loadRecentScores();
    if (scores.empty()) {
        drawText(WIN_W/2 - 140, WIN_H - 170, "No scores yet!");
    } else {
        for (size_t i = 0; i < scores.size(); ++i) {
            std::ostringstream ss; ss << (i+1) << ". " << scores[i];
            drawText(WIN_W/2 - 40, WIN_H - 170 - (int)i*30, ss.str());
        }
    }
    drawText(WIN_W/2 - 180, 40, "Press ESC to return");
}

void renderScene() {
    glClear(GL_COLOR_BUFFER_BIT); // No depth buffer needed for 2D
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glOrtho(0, WIN_W, 0, WIN_H, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();

    if (gameState == GS_MENU) renderMenu();
    else if (gameState == GS_PLAYING || gameState == GS_PAUSED || gameState == GS_LEVEL_CLEAR || gameState == GS_GAMEOVER) {
        renderBricks();
        renderPerks();
        renderProjectiles();
        glColor3f(0.9f,0.9f,0.9f); drawRect(paddle.x, paddle.y, paddle.w, paddle.h);
        
        if (ball.isFireball) glColor3f(1.0f, 0.8f, 0.2f);
        else glColor3f(1.0f,0.4f,0.2f);
        
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(ball.x, ball.y);
        for (int i=0;i<=20;++i) {
            float a = (float)i/20 * 2.0f * M_PI;
            glVertex2f(ball.x + cosf(a)*ball.radius, ball.y + sinf(a)*ball.radius);
        }
        glEnd();
        drawHUD();

        if (gameState == GS_PAUSED) {
            glColor3f(1,0.9f,0.2f); drawText(WIN_W/2 - 40, WIN_H/2, "PAUSED");
        }
        if (gameState == GS_LEVEL_CLEAR) {
            glColor3f(0.9f,0.9f,0.2f); drawText(WIN_W/2 - 70, WIN_H/2 + 20, "LEVEL CLEARED!");
            drawText(WIN_W/2 - 160, WIN_H/2 - 10, "Press SPACE for next level");
        }
        if (gameState == GS_GAMEOVER) {
            glColor3f(1,0.2f,0.2f); drawText(WIN_W/2 - 70, WIN_H/2 + 20, "GAME OVER");
            std::ostringstream ss; ss << "Score: " << score; drawText(WIN_W/2 - 40, WIN_H/2 - 10, ss.str());
            drawText(WIN_W/2 - 160, WIN_H/2 - 40, "Press SPACE to restart");
        }
    } else if (gameState == GS_HELP) renderHelp();
    else if (gameState == GS_SCOREBOARD) renderScoreboard();
    else if (gameState == GS_MUSIC_MENU) {
        glColor3f(1,1,1);
        drawText(WIN_W/2 - 80, WIN_H/2 + 40, "Music Options");
        drawText(WIN_W/2 - 100, WIN_H/2 + 0, "1 - Music ON");
        drawText(WIN_W/2 - 100, WIN_H/2 - 30, "2 - Music OFF");
        drawText(WIN_W/2 - 100, WIN_H/2 - 80, "ESC - Back");
    }

    glutSwapBuffers();
}

// =======================================================
// Part 13: Input Handling
// Details: Mouse clicks, mouse movement, and keyboard presses.
// =======================================================

void mouseClick(int button, int state, int x, int y) {
    if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN) return;
    
    if (gameState == GS_PLAYING) {
        if (fireCooldown <= 0) {
            projectiles.push_back({paddle.x + 10, paddle.y + paddle.h, 500.0f, true});
            projectiles.push_back({paddle.x + paddle.w - 10, paddle.y + paddle.h, 500.0f, true});
            fireCooldown = FIRE_RATE;
        }
    }
}

void passiveMouse(int x, int y) {
    if (gameState == GS_PLAYING) {
        paddle.x = (float)x - paddle.w*0.5f;
        if (ball.stuck) ball.x = paddle.x + paddle.w*0.5f;
    }
}

void keyboardDown(unsigned char key, int, int) {
    if (key == 27) { // ESC key
        if (gameState == GS_MENU) exit(0);
        else gameState = GS_MENU;
    } else if (key == ' ' ) {
        if (gameState == GS_PLAYING && ball.stuck) launchBall();
        else if (gameState == GS_LEVEL_CLEAR) { currentLevel++; startLevel(currentLevel); }
        else if (gameState == GS_GAMEOVER) startNewGame();
    } else if (key == 'p' || key == 'P') {
        if (gameState == GS_PLAYING) gameState = GS_PAUSED;
        else if (gameState == GS_PAUSED) { gameState = GS_PLAYING; gameStartTime = glutGet(GLUT_ELAPSED_TIME) - (int)(elapsedTime*1000.0); }
    } else if (key == 'r' || key == 'R') {
        if (gameState == GS_PLAYING || gameState == GS_PAUSED) startLevel(currentLevel);
    } else if (key == 'a' || key == 'A') keyLeft = true;
    else if (key == 'd' || key == 'D') keyRight = true;
    else if (gameState == GS_MENU) {
        if (key == '1') startNewGame();
        else if (key == '2') gameState = GS_SCOREBOARD;
        else if (key == '3') gameState = GS_MUSIC_MENU;
        else if (key == '4') { openHelpFile(); gameState = GS_HELP; }
    } else if (gameState == GS_MUSIC_MENU) {
        if (key == '1') { playMusic(); gameState = GS_MENU; }
        else if (key == '2') { stopMusic(); gameState = GS_MENU; }
    }
}

void keyboardUp(unsigned char key, int, int) {
    if (key == 'a' || key == 'A') keyLeft = false;
    if (key == 'd' || key == 'D') keyRight = false;
}

void specialDown(int key, int, int) {
    if (key == GLUT_KEY_LEFT) keyLeft = true;
    if (key == GLUT_KEY_RIGHT) keyRight = true;
}
void specialUp(int key, int, int) {
    if (key == GLUT_KEY_LEFT) keyLeft = false;
    if (key == GLUT_KEY_RIGHT) keyRight = false;
}

// =======================================================
// Part 14: Timer Loop
// Details: Computes frame delta, updates game, and schedules redraw.
// =======================================================

void timerFunc(int) {
    static int last = 0;
    int now = glutGet(GLUT_ELAPSED_TIME);
    double dt = (now - last) / 1000.0;
    last = now;
    if (dt > 0.1) dt = 0.1; // Clamp delta time

    if (gameState == GS_PLAYING) updateGame(dt);
    
    glutPostRedisplay();
    glutTimerFunc(16, timerFunc, 0); // Aim for ~60 FPS
}

// =======================================================
// Part 15: Main Entry
// Details: GLUT initialization, setting callbacks, and starting the main loop.
// =======================================================

int main(int argc, char** argv) {
    srand((unsigned)time(NULL));
    highScore = loadHighScore();

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WIN_W, WIN_H);
    glutCreateWindow("DX-Ball Simple");

    // Set a solid dark blue background color
    glClearColor(0.05f, 0.05f, 0.15f, 1.0f);

    resetPaddleAndBall();
    createBricksForLevel(currentLevel);

    glutDisplayFunc(renderScene);
    glutMouseFunc(mouseClick);
    glutPassiveMotionFunc(passiveMouse);
    glutMotionFunc(passiveMouse);
    glutKeyboardFunc(keyboardDown);
    glutKeyboardUpFunc(keyboardUp);
    glutSpecialFunc(specialDown);
    glutSpecialUpFunc(specialUp);
    glutTimerFunc(16, timerFunc, 0);

    glutMainLoop();
    return 0;
}