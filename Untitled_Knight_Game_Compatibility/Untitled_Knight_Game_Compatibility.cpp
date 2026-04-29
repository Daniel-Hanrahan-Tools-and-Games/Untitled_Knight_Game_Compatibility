#include <raylib.h>
#include <vlc/vlc.h>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <cstdint>
#include <cstdio>
#include <cstring>

// ================================================================
// CONFIG
// ================================================================

static constexpr int SCREEN_W = 800;
static constexpr int SCREEN_H = 600;

static constexpr int VIDEO_W = 1280;
static constexpr int VIDEO_H = 720;

static constexpr int STEP_MS = 500;
static constexpr int MAX_COMMANDS = 128;

// ================================================================
// MOD PATH
// ================================================================

static constexpr const char* MOD_PATH =
"./Untitled_Knight_Game_Compatibility_Mod/"
"Untitled_Knight_Game_Compatibility_Mod.lua";

// ================================================================
// STATE
// ================================================================

enum State { STATE_TITLE, STATE_NOTICE, STATE_GAME, STATE_FAIL, STATE_SUCCESS };
enum Cmd { CMD_NONE, CMD_UP, CMD_DOWN, CMD_RIGHT, CMD_ACTION };

struct Command
{
    int step;
    Cmd cmd;
    bool active;
};

struct Game
{
    State state;

    int lastStep = -1;

    bool waitingInput = false;
    Cmd currentCmd = CMD_NONE;
    int inputTimeout = 0;

    bool usingMod = false;

    Command commands[MAX_COMMANDS];
    int commandCount = 0;

    char modVideoPath[256];
    char modTitle[128];
    bool hasModVideo = false;

    Cmd visibleCmd = CMD_NONE;
    bool showPrompt = false;

    int stepProgress = 0;
};

static Game g;

// ================================================================
// VLC
// ================================================================

static libvlc_instance_t* vlc = nullptr;
static libvlc_media_player_t* player = nullptr;

static uint8_t videoBuffer[VIDEO_W * VIDEO_H * 4];
static Texture2D videoTex;

// ================================================================
// COMMAND SYSTEM
// ================================================================

static void AddCommand(int step, Cmd c)
{
    if (g.commandCount >= MAX_COMMANDS) return;
    g.commands[g.commandCount++] = { step, c, true };
}

static Command* GetCommand(int step)
{
    for (int i = 0; i < g.commandCount; i++)
        if (g.commands[i].active && g.commands[i].step == step)
            return &g.commands[i];
    return nullptr;
}

// ================================================================
// DEFAULT TIMELINE (UNCHANGED)
// ================================================================

static void LoadDefault()
{
    g.commandCount = 0;

    AddCommand(2, CMD_DOWN);
    AddCommand(5, CMD_UP);
    AddCommand(8, CMD_ACTION);
    AddCommand(12, CMD_UP);
    AddCommand(15, CMD_ACTION);
    AddCommand(18, CMD_DOWN);
    AddCommand(21, CMD_RIGHT);
    AddCommand(24, CMD_ACTION);
    AddCommand(28, CMD_ACTION);
    AddCommand(32, CMD_ACTION);
    AddCommand(36, CMD_ACTION);
    AddCommand(40, CMD_RIGHT);
    AddCommand(43, CMD_DOWN);
    AddCommand(46, CMD_DOWN);
    AddCommand(49, CMD_ACTION);
    AddCommand(53, CMD_RIGHT);
    AddCommand(56, CMD_RIGHT);
    AddCommand(60, CMD_DOWN);
    AddCommand(62, CMD_ACTION);

    g.usingMod = false;
}

// ================================================================
// VLC CALLBACKS
// ================================================================

static void* Lock(void*, void** p_pixels)
{
    *p_pixels = videoBuffer;
    return nullptr;
}

static void Unlock(void*, void*, void* const*) {}
static void Display(void*, void*) {}

// ================================================================
// VIDEO INIT (FIXED)
// ================================================================

static bool InitVideo(const char* path)
{
    const char* args[] =
    {
        "--no-lua",
        "--no-video-title-show",
        "--no-osd",
        "--quiet"
    };

    vlc = libvlc_new(4, args);
    player = libvlc_media_player_new(vlc);

    if (!vlc || !player)
        return false;

    libvlc_video_set_callbacks(player, Lock, Unlock, Display, nullptr);
    libvlc_video_set_format(player, "RV32", VIDEO_W, VIDEO_H, VIDEO_W * 4);

    libvlc_media_t* media = libvlc_media_new_path(vlc, path);
    libvlc_media_player_set_media(player, media);
    libvlc_media_release(media);

    memset(videoBuffer, 0, sizeof(videoBuffer));

    libvlc_media_player_play(player);
    libvlc_media_player_set_pause(player, 0); // 🔥 IMPORTANT FIX

    Image img = GenImageColor(VIDEO_W, VIDEO_H, BLACK);
    videoTex = LoadTextureFromImage(img);
    UnloadImage(img);

    return true;
}

// ================================================================
// RESET
// ================================================================

static void Reset()
{
    g.lastStep = -1;
    g.waitingInput = false;
    g.visibleCmd = CMD_NONE;
    g.showPrompt = false;
}

// ================================================================
// GAMEPLAY (SAFE)
// ================================================================

static void UpdateGame()
{
    if (!player)
        return;

    long ms = libvlc_media_player_get_time(player);
    int step = ms / STEP_MS;

    g.stepProgress = step;

    if (step != g.lastStep)
    {
        g.lastStep = step;

        for (int i = 0; i < g.commandCount; i++)
        {
            if (g.commands[i].active && g.commands[i].step == step)
            {
                g.waitingInput = true;
                g.visibleCmd = g.commands[i].cmd;
                g.showPrompt = true;
                g.inputTimeout = 30;
            }
        }
    }

    if (g.waitingInput)
    {
        bool ok =
            (g.visibleCmd == CMD_UP && IsKeyPressed(KEY_UP)) ||
            (g.visibleCmd == CMD_DOWN && IsKeyPressed(KEY_DOWN)) ||
            (g.visibleCmd == CMD_RIGHT && IsKeyPressed(KEY_RIGHT)) ||
            (g.visibleCmd == CMD_ACTION && IsKeyPressed(KEY_Z));

        if (ok)
        {
            g.waitingInput = false;
            g.showPrompt = false;
        }
        else if (--g.inputTimeout <= 0)
        {
            g.state = STATE_FAIL;
            g.waitingInput = false;
            g.showPrompt = false;
        }
    }
}

// ================================================================
// UPDATE (UNCHANGED LOGIC)
// ================================================================

static void Update()
{
    switch (g.state)
    {
        case STATE_TITLE:
            if (IsKeyPressed(KEY_Z))
                g.state = STATE_NOTICE;
            break;

        case STATE_NOTICE:
            if (IsKeyPressed(KEY_Y))
            {
                LoadDefault();
                Reset();
                g.state = STATE_GAME;
            }

            if (IsKeyPressed(KEY_N))
            {
                LoadDefault();
                Reset();
                g.state = STATE_GAME;
            }
            break;

        case STATE_GAME:
            UpdateGame();
            break;

        case STATE_FAIL:
            if (IsKeyPressed(KEY_Z))
            {
                LoadDefault();
                Reset();
                g.state = STATE_GAME;
            }
            break;
    }
}

// ================================================================
// DRAW (🔥 FIXED RENDERING)
// ================================================================

static void Draw()
{
    BeginDrawing();
    ClearBackground(BLACK);

    switch (g.state)
    {
        case STATE_TITLE:
            DrawText("Untitled_Knight_Game_Compatibility", 100, 200, 30, WHITE);
            DrawText("PRESS Z", 330, 260, 20, GREEN);
            break;

        case STATE_NOTICE:
            DrawText("Copyright (C) 2025 Daniel",0,10,12,WHITE);
            DrawText("Hanrahan Tools and Games",0,25,12,WHITE);
            DrawText("SPDX-License-Identifier: GPL-3.0-or-later",0,40,12,WHITE);
            DrawText("CC BY-SA 4.0 components included",0,110,12,WHITE);
            DrawText("Y = MOD / N = VANILLA", 260, 200, 20, WHITE);
            break;

        case STATE_GAME:
        {
            // 🔥 CRITICAL FIX: video is now actually drawn
            UpdateTexture(videoTex, videoBuffer);

            DrawTexturePro(
                videoTex,
                {0,0,(float)VIDEO_W,(float)VIDEO_H},
                {0,0,(float)SCREEN_W,(float)SCREEN_H},
                {0,0},
                0,
                WHITE
            );

            DrawText("GAME RUNNING", 20, 20, 20, GREEN);

            if (g.showPrompt)
            {
                const char* t =
                    (g.visibleCmd == CMD_UP) ? "UP" :
                    (g.visibleCmd == CMD_DOWN) ? "DOWN" :
                    (g.visibleCmd == CMD_RIGHT) ? "RIGHT" :
                    "ACTION";

                DrawText("INPUT:", 340, 180, 20, WHITE);
                DrawText(t, 350, 210, 40, YELLOW);
            }
        }
        break;

        case STATE_FAIL:
            DrawText("FAILED - PRESS Z", 300, 300, 20, RED);
            break;
    }

    EndDrawing();
}

// ================================================================
// MAIN
// ================================================================

int main()
{
    InitWindow(SCREEN_W, SCREEN_H, "Game");
    SetTargetFPS(60);

    g.state = STATE_TITLE;

    LoadDefault();
    InitVideo("gameplay_video.ogg");

    while (!WindowShouldClose())
    {
        Update();
        Draw();
    }

    if (player)
        libvlc_media_player_release(player);

    if (vlc)
        libvlc_release(vlc);

    UnloadTexture(videoTex);
    CloseWindow();
    return 0;
}
