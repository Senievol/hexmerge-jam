/*******************************************************************************************
 *
 *   raylib gamejam template
 *
 *
 *   Code licensed under an unmodified zlib/libpng license, which is an OSI-certified,
 *   BSD-like license that allows static linking with closed source software
 *
 *   Copyright (c) 2022-2026 Ramon Santamaria (@raysan5)
 *
 ********************************************************************************************/

#include "raylib.h"

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h> // Emscripten library
#endif

#include <stdio.h>   // Required for: printf()
#include <stdlib.h>  // Required for:
#include <string.h>  // Required for:
#include <math.h>    // Required for: fminf(), fmaxf()
#include <raymath.h> // Required for: Lerp(), Clamp()

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
// Simple log system to avoid printf() calls if required
// NOTE: Avoiding those calls, also avoids const strings memory usage
#define SUPPORT_LOG_INFO
#if defined(SUPPORT_LOG_INFO)
#define LOG(...) printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

#define HEX_MAX_COUNT 240
#define HEX_TIER_COUNT 4
#define HEX_LINE_THICK 4.0f
#define HEX_SPAWN_MARGIN 40.0f
#define HEX_SPAWN_MIN_TIME 0.4f
#define HEX_SPAWN_MAX_TIME 1.0f

#define HEX_GRID_RADIUS 4
#define HEX_SIZE 40.0f

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef enum
{
    SCREEN_LOGO = 0,
    SCREEN_TITLE,
    SCREEN_GAMEPLAY,
    SCREEN_ENDING
} GameScreen;

// Transition: fade out/in
typedef enum
{
    TRANSITION_NONE = 0,
    TRANSITION_OUT, // Fading to black
    TRANSITION_IN   // Fading from black
} TransitionPhase;

typedef struct Hex
{
    bool active;
    Vector2 pos;
    Vector2 vel;    // px/sec
    float rotation; // degrees
    float rotSpeed; // degrees/sec
    int sizeTier;   // index into hexTierSizes
} Hex;

// TODO: Define your custom data types here

//----------------------------------------------------------------------------------
// Global Variables Definition (local to this module)
//----------------------------------------------------------------------------------
static const int screenWidth = 720;
static const int screenHeight = 720;

static RenderTexture2D target = {0}; // Render texture to render our game
static int frameCounter = 0;

static GameScreen currentScreen = SCREEN_TITLE;

// Title screen animation
static float titleAnimTimer = 0.0f;
static const float TITLE_ANIM_DURATION = 0.6f;

// Button hover animation ; 0 not hovered 1 fully hovered
static float playButtonHover = 0.0f;
static float settingsButtonHover = 0.0f;
static float settingsCloseHover = 0.0f;

static bool settingsOpen = false; // Should the pop up be open?
static float popupAnim = 0.0f;

static TransitionPhase transitionPhase = TRANSITION_NONE;
static float transitionTimer = 0.0f;
static const float TRANSITION_DURATION = 0.35f; // seconds per half, out -> in

static const float hexTierSizes[HEX_TIER_COUNT] = {16.0f, 23.0f, 32.0f, 45.0f}; // tier 0..3
static Hex hexes[HEX_MAX_COUNT];
static float hexSpawnTimer = 0.0f;

// TODO: Define global variables here, recommended to make them static

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
static void UpdateDrawFrame(void); // Update and Draw one frame

// Small math helpers
static float EaseOutCubic(float t);
static bool GuiSimpleButton(Rectangle bounds, const char *text, int fontSize, float *hoverAnim, bool interactive);

// Title screen
static void UpdateTitleScreen(float dt);
static void DrawTitleScreen(void);

// Settings popup (overlays the title screen)
static void UpdateSettingsPopup(float dt);
static void DrawSettingsPopup(void);

// Screen transition (title -> gameplay fade)
static void UpdateTransition(float dt);
static void DrawTransitionOverlay(void);
static void StartTransitionToGameplay(void);

// Hex background
static void SpawnHexAt(int index);
static void InitHexBackground(void);
static void UpdateHexBackground(float dt);
static void DrawHexBackground(void);

// Game screen
static Vector2 HexToPixel(int a, int b, float size, Vector2 origin);
static void DrawHexGrid(void);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void)
{
#if !defined(_DEBUG)
    SetTraceLogLevel(LOG_NONE); // Disable raylib trace log messages
#endif

    // Initialization
    //--------------------------------------------------------------------------------------
    InitWindow(screenWidth, screenHeight, "Hexmerge");

    // TODO: Load resources / Initialize variables at this point

    // Render texture to draw, enables screen scaling
    // NOTE: If screen is scaled, mouse input should be scaled proportionally
    target = LoadRenderTexture(screenWidth, screenHeight);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

    InitHexBackground();

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 60, 1);
#else
    SetTargetFPS(60); // Set our game frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose()) // Detect window close button
    {
        UpdateDrawFrame();
    }
#endif

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadRenderTexture(target);

    // TODO: Unload all loaded resources at this point

    CloseWindow(); // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//--------------------------------------------------------------------------------------------
// Module Functions Definition
//--------------------------------------------------------------------------------------------

static float EaseOutCubic(float t)
{
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

static bool GuiSimpleButton(Rectangle bounds, const char *text, int fontSize, float *hoverAnim, bool interactive)
{
    Vector2 mouse = GetMousePosition();
    bool hovered = interactive && CheckCollisionPointRec(mouse, bounds);
    bool clicked = hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    float dt = GetFrameTime();
    float target = hovered ? 1.0f : 0.0f;
    const float hoverSpeed = 7.0f;
    if (*hoverAnim < target)
        *hoverAnim = fminf(*hoverAnim + hoverSpeed * dt, target);
    else
        *hoverAnim = fmaxf(*hoverAnim - hoverSpeed * dt, target);

    // float scale = 1.0f + 0.04f * (*hoverAnim);
    float scale = 1.0f;
    Rectangle drawRect = {
        bounds.x - (bounds.width * (scale - 1.0f)) / 2.0f,
        bounds.y - (bounds.height * (scale - 1.0f)) / 2.0f,
        bounds.width * scale,
        bounds.height * scale};

    Color fillColor, borderColor, textColor;
    if (!interactive)
    {
        fillColor = LIGHTGRAY;
        borderColor = GRAY;
        textColor = GRAY;
    }
    else
    {
        // Color change Raywhite to Lightgray as hover goes up
        fillColor = (Color){
            (unsigned char)Lerp((float)RAYWHITE.r, (float)LIGHTGRAY.r, *hoverAnim),
            (unsigned char)Lerp((float)RAYWHITE.g, (float)LIGHTGRAY.g, *hoverAnim),
            (unsigned char)Lerp((float)RAYWHITE.b, (float)LIGHTGRAY.b, *hoverAnim),
            255};
        borderColor = BLACK;
        textColor = BLACK;
    }

    DrawRectangleRec(drawRect, fillColor);
    DrawRectangleLinesEx(drawRect, 4, borderColor);

    int textWidth = MeasureText(text, fontSize);
    int textX = (int)(drawRect.x + (drawRect.width - textWidth) / 2.0f);
    int textY = (int)(drawRect.y + (drawRect.height - fontSize) / 2.0f);
    DrawText(text, textX, textY, fontSize, textColor);

    return clicked;
}

static void StartTransitionToGameplay(void)
{
    if (transitionPhase == TRANSITION_NONE)
    {
        transitionPhase = TRANSITION_OUT;
        transitionTimer = 0.0f;
    }
}

static void UpdateTransition(float dt)
{
    if (transitionPhase == TRANSITION_OUT)
    {
        transitionTimer += dt;
        if (transitionTimer >= TRANSITION_DURATION)
        {
            currentScreen = SCREEN_GAMEPLAY;
            transitionPhase = TRANSITION_IN;
            transitionTimer = 0.0f;
        }
    }
    else if (transitionPhase == TRANSITION_IN)
    {
        transitionTimer += dt;
        if (transitionTimer >= TRANSITION_DURATION)
        {
            transitionPhase = TRANSITION_NONE;
            transitionTimer = 0.0f;
        }
    }
}

static void DrawTransitionOverlay(void)
{
    if (transitionPhase == TRANSITION_NONE)
        return;

    float alpha = 0.0f;
    if (transitionPhase == TRANSITION_OUT)
        alpha = Clamp(transitionTimer / TRANSITION_DURATION, 0.0f, 1.0f);
    else
        alpha = Clamp(1.0f - transitionTimer / TRANSITION_DURATION, 0.0f, 1.0f);

    DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, alpha));
}

static void UpdateTitleScreen(float dt)
{
    if (titleAnimTimer < TITLE_ANIM_DURATION)
    {
        titleAnimTimer += dt;
        if (titleAnimTimer > TITLE_ANIM_DURATION)
            titleAnimTimer = TITLE_ANIM_DURATION;
    }
}

static void DrawTitleScreen(void)
{
    // Title screen input is disabled while the settings popup is showing/animating
    // or while a screen transition is in progress.
    bool inputBlocked = (transitionPhase != TRANSITION_NONE) || settingsOpen || (popupAnim > 0.01f);

    // Animated 2 color title
    float progress = Clamp(titleAnimTimer / TITLE_ANIM_DURATION, 0.0f, 1.0f);
    float eased = EaseOutCubic(progress);

    int baseFontSize = 90;
    float scale = Lerp(0.7f, 1.0f, eased);
    float alpha = eased;
    int fontSize = (int)(baseFontSize * scale);

    const char *partHex = "Hex";
    const char *partMerge = "merge";

    int hexWidth = MeasureText(partHex, fontSize);
    int mergeWidth = MeasureText(partMerge, fontSize);
    int totalWidth = hexWidth + mergeWidth;

    int titleX = (screenWidth - totalWidth) / 2;
    int titleY = 150;

    DrawText(partHex, titleX, titleY, fontSize, Fade(MAROON, alpha));
    DrawText(partMerge, titleX + hexWidth, titleY, fontSize, Fade(BLACK, alpha));

    // Menu buttons
    const int buttonWidth = 300;
    const int buttonHeight = 70;
    const int buttonSpacing = 30;
    const int buttonX = (screenWidth - buttonWidth) / 2;
    const int firstButtonY = 340;

    Rectangle playButton = {(float)buttonX, (float)firstButtonY, (float)buttonWidth, (float)buttonHeight};
    Rectangle settingsButton = {(float)buttonX, (float)(firstButtonY + (buttonHeight + buttonSpacing)), (float)buttonWidth, (float)buttonHeight};

    if (GuiSimpleButton(playButton, "Play", 32, &playButtonHover, !inputBlocked))
    {
        StartTransitionToGameplay();
    }

    if (GuiSimpleButton(settingsButton, "Settings", 32, &settingsButtonHover, !inputBlocked))
    {
        settingsOpen = true;
    }
}

static void UpdateSettingsPopup(float dt)
{
    float target = settingsOpen ? 1.0f : 0.0f;
    const float popupSpeed = 5.0f;
    if (popupAnim < target)
        popupAnim = fminf(popupAnim + popupSpeed * dt, target);
    else
        popupAnim = fmaxf(popupAnim - popupSpeed * dt, target);
}

static void DrawSettingsPopup(void)
{
    if (popupAnim <= 0.001f)
        return;

    float eased = EaseOutCubic(popupAnim);

    DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.4f * popupAnim));

    Rectangle basePopupRect = {screenWidth / 2.0f - 200, screenHeight / 2.0f - 150, 400, 300};

    float scale = Lerp(0.85f, 1.0f, eased);
    float w = basePopupRect.width * scale;
    float h = basePopupRect.height * scale;
    Rectangle popupRect = {
        basePopupRect.x + (basePopupRect.width - w) / 2.0f,
        basePopupRect.y + (basePopupRect.height - h) / 2.0f,
        w, h};

    DrawRectangleRec(popupRect, Fade(RAYWHITE, eased));
    DrawRectangleLinesEx(popupRect, 4, Fade(BLACK, eased));

    const char *popupTitle = "Settings";
    int popupTitleSize = 36;
    int popupTitleWidth = MeasureText(popupTitle, popupTitleSize);
    DrawText(popupTitle, (int)(popupRect.x + (popupRect.width - popupTitleWidth) / 2.0f),
             (int)(popupRect.y + 20), popupTitleSize, Fade(BLACK, eased));

    Rectangle closeButton = {popupRect.x + popupRect.width / 2 - 100, popupRect.y + popupRect.height - 80, 200, 50};
    bool closeInteractive = (popupAnim > 0.95f);
    if (GuiSimpleButton(closeButton, "Close", 28, &settingsCloseHover, closeInteractive))
    {
        settingsOpen = false;
    }
}

static void SpawnHexAt(int index)
{
    int edge = GetRandomValue(0, 3);
    Vector2 pos, dir;

    switch (edge)
    {
    case 0: // left
        pos = (Vector2){-HEX_SPAWN_MARGIN, (float)GetRandomValue(0, screenHeight)};
        dir = (Vector2){1.0f, (float)GetRandomValue(-40, 40) / 100.0f};
        break;
    case 1: // right
        pos = (Vector2){screenWidth + HEX_SPAWN_MARGIN, (float)GetRandomValue(0, screenHeight)};
        dir = (Vector2){-1.0f, (float)GetRandomValue(-40, 40) / 100.0f};
        break;
    case 2: // top
        pos = (Vector2){(float)GetRandomValue(0, screenWidth), -HEX_SPAWN_MARGIN};
        dir = (Vector2){(float)GetRandomValue(-40, 40) / 100.0f, 1.0f};
        break;
    default: // bottom
        pos = (Vector2){(float)GetRandomValue(0, screenWidth), screenHeight + HEX_SPAWN_MARGIN};
        dir = (Vector2){(float)GetRandomValue(-40, 40) / 100.0f, -1.0f};
        break;
    }

    float speed = (float)GetRandomValue(15, 35);
    hexes[index].active = true;
    hexes[index].pos = pos;
    hexes[index].vel = Vector2Scale(Vector2Normalize(dir), speed);
    hexes[index].rotation = (float)GetRandomValue(0, 359);
    hexes[index].rotSpeed = (float)GetRandomValue(-30, 30);
    hexes[index].sizeTier = 0;
}

static void InitHexBackground(void)
{
    for (int i = 0; i < HEX_MAX_COUNT; i++)
        hexes[i].active = false;

    int startCount = HEX_MAX_COUNT / 3;
    for (int i = 0; i < startCount; i++)
    {
        hexes[i].active = true;
        hexes[i].pos = (Vector2){(float)GetRandomValue(0, screenWidth), (float)GetRandomValue(0, screenHeight)};
        float angle = (float)GetRandomValue(0, 359) * DEG2RAD;
        float speed = (float)GetRandomValue(15, 35);
        hexes[i].vel = (Vector2){cosf(angle) * speed, sinf(angle) * speed};
        hexes[i].rotation = (float)GetRandomValue(0, 359);
        hexes[i].rotSpeed = (float)GetRandomValue(-30, 30);
        hexes[i].sizeTier = 0;
    }

    hexSpawnTimer = HEX_SPAWN_MIN_TIME;
}

static void UpdateHexBackground(float dt)
{
    // Move / rotate / despawn off-screen hexes
    for (int i = 0; i < HEX_MAX_COUNT; i++)
    {
        if (!hexes[i].active)
            continue;

        hexes[i].pos.x += hexes[i].vel.x * dt;
        hexes[i].pos.y += hexes[i].vel.y * dt;
        hexes[i].rotation += hexes[i].rotSpeed * dt;

        float size = hexTierSizes[hexes[i].sizeTier];
        float m = size + HEX_SPAWN_MARGIN;
        if (hexes[i].pos.x < -m || hexes[i].pos.x > screenWidth + m ||
            hexes[i].pos.y < -m || hexes[i].pos.y > screenHeight + m)
        {
            hexes[i].active = false;
        }
    }

    // Merge or bounce checker
    for (int i = 0; i < HEX_MAX_COUNT; i++)
    {
        if (!hexes[i].active)
            continue;
        for (int j = i + 1; j < HEX_MAX_COUNT; j++)
        {
            if (!hexes[j].active)
                continue;

            float sizeI = hexTierSizes[hexes[i].sizeTier];
            float sizeJ = hexTierSizes[hexes[j].sizeTier];
            float dist = Vector2Distance(hexes[i].pos, hexes[j].pos);
            float overlapThreshold = (sizeI + sizeJ) * 0.87f;

            if (dist < overlapThreshold)
            {
                bool sameTier = (hexes[i].sizeTier == hexes[j].sizeTier);
                bool canGrow = (hexes[i].sizeTier < HEX_TIER_COUNT - 1);

                if (sameTier && canGrow)
                {
                    // Merge (ie. promote to upper tier)
                    hexes[i].pos = Vector2Lerp(hexes[i].pos, hexes[j].pos, 0.5f);
                    hexes[i].vel = Vector2Scale(Vector2Add(hexes[i].vel, hexes[j].vel), 0.5f);
                    hexes[i].rotSpeed = (hexes[i].rotSpeed + hexes[j].rotSpeed) * 0.5f;
                    hexes[i].sizeTier++;

                    hexes[j].active = false;
                }
                else
                {
                    // Not the same tier colliding or max tier colliding with any other -> bounce
                    Vector2 normal = (dist > 0.0001f)
                                         ? Vector2Scale(Vector2Subtract(hexes[j].pos, hexes[i].pos), 1.0f / dist)
                                         : (Vector2){1.0f, 0.0f};

                    float v1n = Vector2DotProduct(hexes[i].vel, normal);
                    float v2n = Vector2DotProduct(hexes[j].vel, normal);

                    Vector2 tangentI = Vector2Subtract(hexes[i].vel, Vector2Scale(normal, v1n));
                    Vector2 tangentJ = Vector2Subtract(hexes[j].vel, Vector2Scale(normal, v2n));

                    hexes[i].vel = Vector2Add(tangentI, Vector2Scale(normal, v2n));
                    hexes[j].vel = Vector2Add(tangentJ, Vector2Scale(normal, v1n));

                    float pushAmount = (overlapThreshold - dist) * 0.5f + 0.5f;
                    hexes[i].pos = Vector2Subtract(hexes[i].pos, Vector2Scale(normal, pushAmount));
                    hexes[j].pos = Vector2Add(hexes[j].pos, Vector2Scale(normal, pushAmount));
                }
            }
        }
    }

    // Spawn new hexes over time
    hexSpawnTimer -= dt;
    if (hexSpawnTimer <= 0.0f)
    {
        for (int i = 0; i < HEX_MAX_COUNT; i++)
        {
            if (!hexes[i].active)
            {
                SpawnHexAt(i);
                break;
            }
        }
        hexSpawnTimer = (float)GetRandomValue((int)(HEX_SPAWN_MIN_TIME * 100), (int)(HEX_SPAWN_MAX_TIME * 100)) / 100.0f;
    }
}

static void DrawHexBackground(void)
{
    Color hexColor = Fade(BLACK, 0.08f);

    for (int i = 0; i < HEX_MAX_COUNT; i++)
    {
        if (!hexes[i].active)
            continue;
        float size = hexTierSizes[hexes[i].sizeTier];
        DrawPolyLinesEx(hexes[i].pos, 6, size, hexes[i].rotation, HEX_LINE_THICK, hexColor);
    }
}

static Vector2 HexToPixel(int a, int b, float size, Vector2 origin)
{
    float x = size * 1.5f * (float)a;
    float y = size * sqrtf(3.0f) * ((float)b + (float)a / 2.0f);

    Vector2 rotated = Vector2Rotate((Vector2){x, y}, 30.0f * DEG2RAD);

    return (Vector2){origin.x + rotated.x, origin.y + rotated.y};
}

static void DrawHexGrid(void)
{
    Vector2 origin = {screenWidth / 2.0f, (screenHeight / 2.0f) - 75.0f };

    for (int a = -HEX_GRID_RADIUS; a <= HEX_GRID_RADIUS; a++)
    {
        int bMin = -HEX_GRID_RADIUS;
        if (-a - HEX_GRID_RADIUS > bMin)
        {
            bMin = -a - HEX_GRID_RADIUS;
        }

        int bMax = HEX_GRID_RADIUS;
        if (-a + HEX_GRID_RADIUS < bMax)
        {
            bMax = -a + HEX_GRID_RADIUS;
        }

        for (int b = bMin; b <= bMax; b++)
        {
            Vector2 center = HexToPixel(a, b, HEX_SIZE - 2, origin);

            DrawPolyLinesEx(center, 6, HEX_SIZE, 30.0f, HEX_LINE_THICK, Fade(LIME, 0.08f));
        }
    }
}

// Update and draw frame
void UpdateDrawFrame(void)
{
    // Update
    //----------------------------------------------------------------------------------
    frameCounter++;
    float dt = GetFrameTime();

    switch (currentScreen)
    {
    case SCREEN_TITLE:
    {
        UpdateTitleScreen(dt);
        UpdateHexBackground(dt);
    }
    break;

    case SCREEN_GAMEPLAY:
    {
        // TODO: Gameplay
    }
    break;

    case SCREEN_LOGO:
    case SCREEN_ENDING:
    default:
        break;
    }

    UpdateSettingsPopup(dt);
    UpdateTransition(dt);
    //----------------------------------------------------------------------------------
    // Draw
    //----------------------------------------------------------------------------------
    // Render game screen to a texture,
    // it could be useful for scaling or further shader postprocessing
    BeginTextureMode(target);
    ClearBackground(RAYWHITE);

    switch (currentScreen)
    {
    case SCREEN_TITLE:
    {
        DrawTitleScreen();
        DrawHexBackground();
    }
    break;

    case SCREEN_GAMEPLAY:
    {
        // TODO: Gameplay
        DrawHexGrid();
    }
    break;

    case SCREEN_LOGO:
    case SCREEN_ENDING:
    default:
        break;
    }

    DrawSettingsPopup();

    DrawTransitionOverlay();

    // DrawRectangleLinesEx((Rectangle){0, 0, screenWidth, screenHeight}, 16, BLACK);

    EndTextureMode();

    // Render to screen (main framebuffer)
    BeginDrawing();
    ClearBackground(RAYWHITE);

    // Draw render texture to screen, scaled if required
    DrawTexturePro(target.texture, (Rectangle){0, 0, (float)target.texture.width, -(float)target.texture.height},
                   (Rectangle){0, 0, (float)target.texture.width, (float)target.texture.height}, (Vector2){0, 0}, 0.0f, WHITE);

    // TODO: Draw everything that requires to be drawn at this point, maybe UI?

    EndDrawing();
    //----------------------------------------------------------------------------------
}