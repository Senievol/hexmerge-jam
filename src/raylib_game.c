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

#define GLSL_VERSION 100

#define HEX_MAX_COUNT 240
#define HEX_TIER_COUNT 4
#define HEX_LINE_THICK 4.0f
#define HEX_SPAWN_MARGIN 40.0f
#define HEX_SPAWN_MIN_TIME 0.4f
#define HEX_SPAWN_MAX_TIME 1.0f

#define HEX_GRID_RADIUS 4
#define TILE_SIZE 40.0f

#define ICON_BUTTON_SIZE 56.0f
#define ICON_BUTTON_MARGIN 20.0f

#define CORE_MAX_HEALTH 100
#define CORE_DAMAGE_PER_HIT 10
#define CORE_HIT_FLASH_DURATION 0.25f

#define ENEMY_MAX_COUNT 128        // Fixed size
#define ENEMY_SPAWN_MIN_TIME 1.2f
#define ENEMY_SPAWN_MAX_TIME 2.5f
#define ENEMY_BASE_SPEED 55.0f     // px/sec
#define ENEMY_BASE_HEALTH 20.0f
#define ENEMY_RADIUS 14.0f         // Placeholder circle radius (until sprite is added)
#define ENEMY_ARRIVE_THRESHOLD 2.0f

#define HEX_RING_TILE_COUNT (HEX_GRID_RADIUS * 6)

#define NULL_TILE (Vector2){99, 99}
#define IS_NULL_TILE(tile) (tile.x == NULL_TILE.x && tile.y == NULL_TILE.y)

#define RGB(r, g, b) (Color){r, g, b, 255}
#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)

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

// Icon shown inside a GuiIconButton
typedef enum
{
    ICON_HELP = 0,
    ICON_SETTINGS
} IconType;

typedef struct Hex
{
    bool active;
    Vector2 pos;
    Vector2 vel;    // px/sec
    float rotation; // degrees
    float rotSpeed; // degrees/sec
    int sizeTier;   // index into hexTierSizes
} Hex;

typedef struct Tower
{
    int x;
    int y;
    int type;
} Tower;

typedef struct Core
{
    int health;
    int maxHealth;
    int level;       // TODO: Upgrade system
    float hitFlashT; // Flash duration when damaged
} Core;

typedef struct Enemy
{
    bool active; // Fixed-size pool slot in use?

    int hexA, hexB;
    int targetA, targetB;

    Vector2 worldPos;

    float speed;    // px/sec
    float health;   // current health
    float maxHealth;

    Texture2D texture; // for the sprite
} Enemy;

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

// Gameplay top bar icon buttons (How To Play / Settings)
static float howToPlayButtonHover = 0.0f;
static float settingsIconButtonHover = 0.0f;
static float howToPlayCloseHover = 0.0f;

static bool howToPlayOpen = false; // Should the How To Play pop up be open?
static float howToPlayAnim = 0.0f;

static TransitionPhase transitionPhase = TRANSITION_NONE;
static float transitionTimer = 0.0f;
static const float TRANSITION_DURATION = 0.35f; // seconds per half, out -> in
static GameScreen transitionTargetScreen = SCREEN_GAMEPLAY; // Which screen to switch to once the fade-out completes

// Ending screen buttons
static float endingRestartHover = 0.0f;
static float endingMenuHover = 0.0f;

static const float hexTierSizes[HEX_TIER_COUNT] = {16.0f, 23.0f, 32.0f, 45.0f}; // tier 0..3
static Hex hexes[HEX_MAX_COUNT];
static float hexSpawnTimer = 0.0f;

static const char *blurFShaderSrc100 =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec2 fragTexCoord;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec2 resolution;\n"
    "uniform float blurSize;\n"
    "void main()\n"
    "{\n"
    "    vec2 texel = 1.0 / resolution;\n"
    "    vec4 color = vec4(0.0);\n"
    "    float samples = 0.0;\n"
    "    for (int x = -2; x <= 2; x++)\n"
    "    for (int y = -2; y <= 2; y++)\n"
    "    {\n"
    "        color += texture2D(texture0, fragTexCoord + vec2(float(x), float(y)) * texel * blurSize);\n"
    "        samples += 1.0;\n"
    "    }\n"
    "    gl_FragColor = color / samples;\n"
    "}\n";

// Game Interface
static Vector2 canvasOrigin = {screenWidth / 2.0f, (screenHeight / 2.0f) - 60.0f};
static int draggedTowerId = 0;

// Game objects
static Tower towers[256];
static int towerCount = 0;

static Core core = {0};

static Enemy enemies[ENEMY_MAX_COUNT];
static float enemySpawnTimer = 0.0f;

static Vector2 hexRingTiles[HEX_RING_TILE_COUNT];
static int hexRingTileCount = 0;

static const int hexDirQ[6] = {+1, +1, 0, -1, -1, 0};
static const int hexDirR[6] = {0, -1, -1, 0, +1, +1};

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
static void UpdateDrawFrame(void); // Update and Draw one frame

// Small math helpers
static float EaseOutCubic(float t);
static bool GuiSimpleButton(Rectangle bounds, const char *text, int fontSize, float *hoverAnim, bool interactive);
static bool GuiIconButton(Rectangle bounds, IconType icon, float *hoverAnim, bool interactive);
static void DrawSlidersIcon(Vector2 center, float size, Color lineColor, Color knobFillColor);

// Title screen
static void UpdateTitleScreen(float dt);
static void DrawTitleScreen(void);

// Settings popup (overlays the title screen or gameplay screen)
static void UpdateSettingsPopup(float dt);
static void DrawSettingsPopup(void);

// How To Play popup (overlays the gameplay screen)
static void UpdateHowToPlayPopup(float dt);
static void DrawHowToPlayPopup(void);

// Screen transition (fade out -> switch screen -> fade in)
static void UpdateTransition(float dt);
static void DrawTransitionOverlay(void);
static void StartTransition(GameScreen targetScreen);
static void StartTransitionToGameplay(void); // convenience wrapper (Play button), kept for existing call sites

// Hex background
static void SpawnHexAt(int index);
static void InitHexBackground(void);
static void UpdateHexBackground(float dt);
static void DrawHexBackground(void);
static RenderTexture2D hexLayer = {0};
static Shader blurShader = {0};
static int blurResolutionLoc = -1;
static int blurSizeLoc = -1;

// Game screen
static Vector2 HexToPix(int a, int b, float size, Vector2 origin);
static Vector2 PixToHex(int x, int y, float size, Vector2 origin);
static void DrawHexGrid(void);
static void DrawInventory(void);
static void DrawTowers(void);
static void DrawGameplayTopBar(bool interactive);

// Gameplay
static void DrawTower(int a, int b, int type);
static Vector2 DraggableTower(int x, int y, int type, int id);

// Pathfinding helper
static int HexDistance(int a1, int b1, int a2, int b2);
static Vector2 GetNextStepTowardCore(int a, int b);

// Core
static void InitCore(void);
static void UpdateCore(float dt);
static void DrawCore(void);
static void DamageCore(int amount);

// Enemy system
static void InitEnemies(void);
static void SpawnEnemy(void);
static void UpdateEnemies(float dt);
static void DrawEnemies(void);

// Ending screen
static void ResetGame(void);
static void DrawEndingScreen(void);
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

    hexLayer = LoadRenderTexture(screenWidth, screenHeight);
    SetTextureFilter(hexLayer.texture, TEXTURE_FILTER_BILINEAR);

    blurShader = LoadShaderFromMemory(NULL, blurFShaderSrc100);
    blurResolutionLoc = GetShaderLocation(blurShader, "resolution");
    blurSizeLoc = GetShaderLocation(blurShader, "blurSize");

    float res[2] = {(float)screenWidth, (float)screenHeight};
    SetShaderValue(blurShader, blurResolutionLoc, res, SHADER_UNIFORM_VEC2);
    float blurAmount = 1.0f; // strengthen blur
    SetShaderValue(blurShader, blurSizeLoc, &blurAmount, SHADER_UNIFORM_FLOAT);

    InitHexBackground();
    InitCore();
    InitEnemies();

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
    UnloadRenderTexture(hexLayer);
    UnloadShader(blurShader);
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

static void DrawSlidersIcon(Vector2 center, float size, Color lineColor, Color knobFillColor)
{
    const int lineCount = 3;
    const float halfW = size * 0.5f;
    const float lineThick = size * 0.11f;
    const float knobRadius = size * 0.13f;
    const float knobRingThick = size * 0.03f;
    const float knobPos[lineCount] = {0.32f, 0.68f, 0.5f};
 
    for (int i = 0; i < lineCount; i++)
    {
        float y = center.y - size * 0.5f + size * ((float)(i + 1) / (lineCount + 1));
 
        Vector2 start = {center.x - halfW, y};
        Vector2 end = {center.x + halfW, y};
        DrawLineEx(start, end, lineThick, lineColor);
 
        Vector2 knobCenter = {start.x + (end.x - start.x) * knobPos[i], y};
        Rectangle knobRect = {
            knobCenter.x - knobRadius, knobCenter.y - knobRadius,
            knobRadius * 2.0f, knobRadius * 2.0f};
 
        DrawRectangleRec(knobRect, knobFillColor);
        DrawRectangleLinesEx(knobRect, knobRingThick, lineColor);
    }
}


static bool GuiIconButton(Rectangle bounds, IconType icon, float *hoverAnim, bool interactive)
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

    Color fillColor, borderColor, iconColor;
    if (!interactive)
    {
        fillColor = LIGHTGRAY;
        borderColor = GRAY;
        iconColor = GRAY;
    }
    else
    {
        fillColor = (Color){
            (unsigned char)Lerp((float)RAYWHITE.r, (float)LIGHTGRAY.r, *hoverAnim),
            (unsigned char)Lerp((float)RAYWHITE.g, (float)LIGHTGRAY.g, *hoverAnim),
            (unsigned char)Lerp((float)RAYWHITE.b, (float)LIGHTGRAY.b, *hoverAnim),
            255};
        borderColor = BLACK;
        iconColor = BLACK;
    }

    DrawRectangleRec(bounds, fillColor);
    DrawRectangleLinesEx(bounds, 3, borderColor);

    Vector2 center = {bounds.x + bounds.width / 2.0f, bounds.y + bounds.height / 2.0f};

    if (icon == ICON_HELP)
    {
        int fontSize = (int)(bounds.height * 0.68f);
        int textWidth = MeasureText("?", fontSize);
        DrawText("?", (int)(center.x - textWidth / 2.0f), (int)(center.y - fontSize / 2.0f), fontSize, iconColor);
    }
    else // ICON_SETTINGS
    {
        DrawSlidersIcon(center, bounds.width * 0.78f, iconColor, fillColor);
    }

    return clicked;
}

static void StartTransition(GameScreen targetScreen)
{
    if (transitionPhase == TRANSITION_NONE)
    {
        transitionTargetScreen = targetScreen;
        transitionPhase = TRANSITION_OUT;
        transitionTimer = 0.0f;
    }
}

static void StartTransitionToGameplay(void)
{
    StartTransition(SCREEN_GAMEPLAY);
}

static void UpdateTransition(float dt)
{
    if (transitionPhase == TRANSITION_OUT)
    {
        transitionTimer += dt;
        if (transitionTimer >= TRANSITION_DURATION)
        {
            currentScreen = transitionTargetScreen; // Switch to whichever screen was requested
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
    // Title screen input is disabled while the settings popup is showing/animating or while a screen transition is in progress.
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

static void UpdateHowToPlayPopup(float dt)
{
    float target = howToPlayOpen ? 1.0f : 0.0f;
    const float popupSpeed = 5.0f;
    if (howToPlayAnim < target)
        howToPlayAnim = fminf(howToPlayAnim + popupSpeed * dt, target);
    else
        howToPlayAnim = fmaxf(howToPlayAnim - popupSpeed * dt, target);
}

static void DrawHowToPlayPopup(void)
{
    if (howToPlayAnim <= 0.001f)
        return;

    float eased = EaseOutCubic(howToPlayAnim);

    DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.4f * howToPlayAnim));

    Rectangle basePopupRect = {screenWidth / 2.0f - 260, screenHeight / 2.0f - 230, 520, 460};

    float scale = Lerp(0.85f, 1.0f, eased);
    float w = basePopupRect.width * scale;
    float h = basePopupRect.height * scale;
    Rectangle popupRect = {
        basePopupRect.x + (basePopupRect.width - w) / 2.0f,
        basePopupRect.y + (basePopupRect.height - h) / 2.0f,
        w, h};

    DrawRectangleRec(popupRect, Fade(RAYWHITE, eased));
    DrawRectangleLinesEx(popupRect, 4, Fade(BLACK, eased));

    const char *popupTitle = "How To Play";
    int popupTitleSize = 32;
    int popupTitleWidth = MeasureText(popupTitle, popupTitleSize);
    DrawText(popupTitle, (int)(popupRect.x + (popupRect.width - popupTitleWidth) / 2.0f),
             (int)(popupRect.y + 20), popupTitleSize, Fade(BLACK, eased));

    // TODO: Replace this placeholder
    const char *helpText =
        "Lorem ipsum dolor sit amet, consectetur dddddddddddd\n"
        "adipiscing elit. Sed do eiusmod tempor\n"
        "incididunt ut labore et dolore magna aliqua.\n"
        "\n"
        "Ut enim ad minim veniam, quis nostrud\n"
        "exercitation ullamco laboris nisi ut aliquip\n"
        "ex ea commodo consequat.\n"
        "\n"
        "Duis aute irure dolor in reprehenderit in\n"
        "voluptate velit esse cillum dolore eu fugiat\n"
        "nulla pariatur excepteur sint occaecat.";

    DrawText(helpText, (int)(popupRect.x + 24), (int)(popupRect.y + 72), 18, Fade(BLACK, eased));

    Rectangle closeButton = {popupRect.x + popupRect.width / 2 - 100, popupRect.y + popupRect.height - 70, 200, 50};
    bool closeInteractive = (howToPlayAnim > 0.95f);
    if (GuiSimpleButton(closeButton, "Close", 28, &howToPlayCloseHover, closeInteractive))
    {
        howToPlayOpen = false;
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

static Vector2 HexToPix(int a, int b, float size, Vector2 origin)
{
    float x = size * 1.5f * (float)a;
    float y = size * sqrtf(3.0f) * ((float)b + (float)a / 2.0f);

    Vector2 rotated = Vector2Rotate((Vector2){x, y}, 30.0f * DEG2RAD);

    return (Vector2){origin.x + rotated.x, origin.y + rotated.y};
}

static Vector2 PixToHex(int x, int y, float size, Vector2 origin)
{
    float dist = INFINITY;
    float af, bf;

    // go over the whole grid to find which tile is the closest :skull:
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
            Vector2 center = HexToPix(a, b, TILE_SIZE, canvasOrigin);

            if ((center.x - x) * (center.x - x) + (center.y - y) * (center.y - y) < dist)
            {
                dist = (center.x - x) * (center.x - x) + (center.y - y) * (center.y - y);
                af = a;
                bf = b;
            }
        }
    }

    return (Vector2){af, bf};
}

// Distance between two tiles in axial hex coordinates.
// Standard formula: (|dq| + |dr| + |dq + dr|) / 2
static int HexDistance(int a1, int b1, int a2, int b2)
{
    int dq = a1 - a2;
    int dr = b1 - b2;
    return (abs(dq) + abs(dr) + abs(dq + dr)) / 2;
}

// Greedy pathfinding, could be replaced by A* or anything else like that
static Vector2 GetNextStepTowardCore(int a, int b)
{
    int currentDist = HexDistance(a, b, 0, 0);
    if (currentDist == 0)
        return (Vector2){(float)a, (float)b}; // Already at the core

    for (int dir = 0; dir < 6; dir++)
    {
        int na = a + hexDirQ[dir];
        int nb = b + hexDirR[dir];
        if (HexDistance(na, nb, 0, 0) < currentDist)
            return (Vector2){(float)na, (float)nb};
    }

    // Security but should be unreachable on the grid
    return (Vector2){(float)a, (float)b};
}

static void DrawHexGrid(void)
{
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
            Vector2 center = HexToPix(a, b, TILE_SIZE, canvasOrigin);

            DrawPolyLinesEx(center, 6, TILE_SIZE + 2, 30.0f, HEX_LINE_THICK, RGB(157, 237, 181));
        }
    }
}

static void DrawTower(int a, int b, int type)
{
    Vector2 pos = HexToPix(a, b, TILE_SIZE, canvasOrigin);
    DrawPoly(pos, 6, TILE_SIZE + 2, 30.0f, Fade(RGB(157, 237, 181), 0.45f));

    Color color = RGB(251, 84, 43);
    switch (type)
    {
    case 1:
        color = RGB(91, 41, 126);
        break;
    case 2:
        color = RGB(41, 72, 126);
        break;
    case 3:
        color = RGB(38, 109, 49);
        break;
    case 4:
        color = RGB(196, 108, 16);
        break;
    default:
        break;
    }
    DrawPoly(pos, 3, TILE_SIZE * 0.7f, 30.0f, color);
}

static Vector2 DraggableTower(int x, int y, int type, int id)
{
    Rectangle hitbox = {x - 50, y - 50, 100, 80};
    // DrawRectangleRec(hitbox, Fade(BLACK, 0.1f)); // show hitbox to debug

    Color color = RGB(251, 84, 43);
    switch (type)
    {
    case 1:
        color = RGB(91, 41, 126);
        break;
    case 2:
        color = RGB(41, 72, 126);
        break;
    case 3:
        color = RGB(38, 109, 49);
        break;
    case 4:
        color = RGB(196, 108, 16);
        break;
    default:
        break;
    }

    // detect mouse in bounds, drag, and return the position on the grid when the mouse is released

    Vector2 mouse = GetMousePosition();
    if (draggedTowerId == id)
    {
        Vector2 pos = PixToHex(mouse.x, mouse.y, TILE_SIZE, canvasOrigin);
        if (mouse.y < screenHeight - 120)
            DrawTower(pos.x, pos.y, type);
        else
        {
            x = (int)mouse.x;
            y = (int)mouse.y;
        }
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
            draggedTowerId = 0;
            if (mouse.y < screenHeight - 120) // when released above the inventory, return position (tower placed)
                return pos;
        }
    }
    else
    {
        if (CheckCollisionPointRec(GetMousePosition(), hitbox))
        {
            color.a = 230;
            DrawPoly((Vector2){x, y}, 3, 48.0f, 30.0f, WHITE);
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                draggedTowerId = id;
        }
    }
    if (mouse.y >= screenHeight - 120 || draggedTowerId != id)
        DrawPoly((Vector2){x, y}, 3, 48.0f, 30.0f, color);

    return NULL_TILE;
}

static void DrawInventory(void)
{
    Rectangle inventoryRect = {0, screenHeight - 120, screenWidth, 120};
    DrawRectangleRec(inventoryRect, RGB(138, 159, 179));
    DrawRectangleLinesEx(inventoryRect, 6, DARKGRAY);

    float itemsY = inventoryRect.y + inventoryRect.height / 2 + 10;
    Vector2 tile;
    for (int i = 0; i < 5; i++)
    {
        int type = i;
        int id = i + 1;
        tile = DraggableTower(96 + 132 * i, itemsY, type, id);
        if (!IS_NULL_TILE(tile))
        {
            towers[towerCount++] = (Tower){tile.x, tile.y, type};
        }
    }
}

static void DrawTowers(void)
{
    Tower tower;
    for (int i = 0; i < towerCount; i++)
    {
        tower = towers[i];
        DrawTower(tower.x, tower.y, tower.type);
    }
}

static void InitCore(void)
{
    core.health = CORE_MAX_HEALTH;
    core.maxHealth = CORE_MAX_HEALTH;
    core.level = 1;
    core.hitFlashT = 0.0f;
}

static void DamageCore(int amount)
{
    core.health -= amount;
    if (core.health < 0)
        core.health = 0;
    core.hitFlashT = CORE_HIT_FLASH_DURATION;
}

static void UpdateCore(float dt)
{
    if (core.hitFlashT > 0.0f)
    {
        core.hitFlashT -= dt;
        if (core.hitFlashT < 0.0f)
            core.hitFlashT = 0.0f;
    }

    // TODO: Upgrades (more damage, more range etc...)

    if (core.health <= 0)
    {
        StartTransition(SCREEN_ENDING);
    }
}

static void DrawCore(void)
{
    Vector2 pos = HexToPix(0, 0, TILE_SIZE, canvasOrigin);

    // Flash red when hit else greyish
    float flash = core.hitFlashT / CORE_HIT_FLASH_DURATION;
    Color baseColor = RGB(70, 70, 90);
    Color flashColor = RGB(220, 40, 40);
    Color coreColor = {
        (unsigned char)Lerp((float)baseColor.r, (float)flashColor.r, flash),
        (unsigned char)Lerp((float)baseColor.g, (float)flashColor.g, flash),
        (unsigned char)Lerp((float)baseColor.b, (float)flashColor.b, flash),
        255};

    // TODO: Replace by a sprite with DrawTexturePro
    DrawPoly(pos, 6, TILE_SIZE * 0.8f, 30.0f, coreColor);
    DrawPolyLinesEx(pos, 6, TILE_SIZE * 0.8f, 30.0f, HEX_LINE_THICK, BLACK);

    // Health bar above the core
    float barWidth = TILE_SIZE * 1.6f;
    float barHeight = 8.0f;
    Rectangle barBg = {pos.x - barWidth / 2.0f, pos.y - TILE_SIZE - 20.0f, barWidth, barHeight};
    float healthRatio = (core.maxHealth > 0) ? ((float)core.health / (float)core.maxHealth) : 0.0f;
    Rectangle barFg = barBg;
    barFg.width *= Clamp(healthRatio, 0.0f, 1.0f);

    DrawRectangleRec(barBg, Fade(BLACK, 0.35f));
    DrawRectangleRec(barFg, RGB(60, 200, 90));
    DrawRectangleLinesEx(barBg, 2, BLACK);
}

static void DrawGameplayTopBar(bool interactive)
{
    Rectangle howToPlayButton = {ICON_BUTTON_MARGIN, ICON_BUTTON_MARGIN, ICON_BUTTON_SIZE, ICON_BUTTON_SIZE};
    if (GuiIconButton(howToPlayButton, ICON_HELP, &howToPlayButtonHover, interactive))
    {
        howToPlayOpen = true;
    }

    Rectangle settingsIconButton = {
        screenWidth - ICON_BUTTON_MARGIN - ICON_BUTTON_SIZE,
        ICON_BUTTON_MARGIN,
        ICON_BUTTON_SIZE,
        ICON_BUTTON_SIZE};
    if (GuiIconButton(settingsIconButton, ICON_SETTINGS, &settingsIconButtonHover, interactive))
    {
        settingsOpen = true;
    }
}

static void InitEnemies(void)
{
    for (int i = 0; i < ENEMY_MAX_COUNT; i++)
        enemies[i].active = false;

    hexRingTileCount = 0;
    for (int a = -HEX_GRID_RADIUS; a <= HEX_GRID_RADIUS; a++)
    {
        int bMin = -HEX_GRID_RADIUS;
        if (-a - HEX_GRID_RADIUS > bMin)
            bMin = -a - HEX_GRID_RADIUS;

        int bMax = HEX_GRID_RADIUS;
        if (-a + HEX_GRID_RADIUS < bMax)
            bMax = -a + HEX_GRID_RADIUS;

        for (int b = bMin; b <= bMax; b++)
        {
            if (HexDistance(a, b, 0, 0) == HEX_GRID_RADIUS && hexRingTileCount < HEX_RING_TILE_COUNT)
            {
                hexRingTiles[hexRingTileCount++] = (Vector2){(float)a, (float)b};
            }
        }
    }

    enemySpawnTimer = ENEMY_SPAWN_MIN_TIME;
}

// Spawn a monster on one of the outer side tile
static void SpawnEnemy(void)
{
    if (hexRingTileCount == 0)
        return; // safety check

    int slot = -1;
    for (int i = 0; i < ENEMY_MAX_COUNT; i++)
    {
        if (!enemies[i].active)
        {
            slot = i;
            break;
        }
    }
    if (slot == -1)
        return; // full -> skip

    Vector2 spawnTile = hexRingTiles[GetRandomValue(0, hexRingTileCount - 1)];
    int a = (int)spawnTile.x;
    int b = (int)spawnTile.y;

    Enemy *e = &enemies[slot];
    e->active = true;
    e->hexA = a;
    e->hexB = b;
    e->worldPos = HexToPix(a, b, TILE_SIZE, canvasOrigin);
    e->speed = ENEMY_BASE_SPEED;
    e->maxHealth = ENEMY_BASE_HEALTH;
    e->health = ENEMY_BASE_HEALTH;
    e->texture = (Texture2D){0}; // TODO: assign a loaded sprite once available

    Vector2 next = GetNextStepTowardCore(a, b);
    e->targetA = (int)next.x;
    e->targetB = (int)next.y;
}

// Spawning, movement between tiles, path & core damage if at the core
static void UpdateEnemies(float dt)
{
    // Spawn timer
    enemySpawnTimer -= dt;
    if (enemySpawnTimer <= 0.0f)
    {
        SpawnEnemy();
        enemySpawnTimer = (float)GetRandomValue((int)(ENEMY_SPAWN_MIN_TIME * 100), (int)(ENEMY_SPAWN_MAX_TIME * 100)) / 100.0f;
    }

    for (int i = 0; i < ENEMY_MAX_COUNT; i++)
    {
        Enemy *e = &enemies[i];
        if (!e->active)
            continue;
        // Measure the gap
        Vector2 targetWorld = HexToPix(e->targetA, e->targetB, TILE_SIZE, canvasOrigin);
        Vector2 toTarget = Vector2Subtract(targetWorld, e->worldPos);
        float dist = Vector2Length(toTarget);

        if (dist <= ENEMY_ARRIVE_THRESHOLD)
        {
            // Arrived or still walking?
            e->hexA = e->targetA;
            e->hexB = e->targetB;
            e->worldPos = targetWorld;

            if (e->hexA == 0 && e->hexB == 0)
            {
                // Reached the core? Damage then dies
                DamageCore(CORE_DAMAGE_PER_HIT);
                e->active = false;
                continue;
            }

            Vector2 next = GetNextStepTowardCore(e->hexA, e->hexB);
            e->targetA = (int)next.x;
            e->targetB = (int)next.y;
        }
        else
        {
            Vector2 dir = Vector2Scale(toTarget, 1.0f / dist); // normalize
            float step = e->speed * dt; // how far can it move next frame
            if (step > dist)
                step = dist; // clamp so no overshoot
            e->worldPos = Vector2Add(e->worldPos, Vector2Scale(dir, step));
        }
    }
}

static void DrawEnemies(void)
{
    for (int i = 0; i < ENEMY_MAX_COUNT; i++)
    {
        Enemy *e = &enemies[i];
        if (!e->active)
            continue;

        // TODO: Replace placeholder circle to a sprite with DrawTexturePro
        DrawCircleV(e->worldPos, ENEMY_RADIUS, RGB(200, 40, 40));
        DrawCircleLines((int)e->worldPos.x, (int)e->worldPos.y, ENEMY_RADIUS, BLACK);

        // Health bar above each enemy
        float ratio = (e->maxHealth > 0) ? (e->health / e->maxHealth) : 0.0f;
        float w = ENEMY_RADIUS * 2.0f;
        Rectangle hpBg = {e->worldPos.x - w / 2.0f, e->worldPos.y - ENEMY_RADIUS - 10.0f, w, 4.0f};
        Rectangle hpFg = hpBg;
        hpFg.width *= Clamp(ratio, 0.0f, 1.0f);
        DrawRectangleRec(hpBg, Fade(BLACK, 0.4f));
        DrawRectangleRec(hpFg, RGB(60, 200, 90));
    }
}

static void ResetGame(void)
{
    InitCore();
    InitEnemies();

    towerCount = 0;
    draggedTowerId = 0;

    // "Reset" the button after a screen_XXX switch
    endingRestartHover = 0.0f;
    endingMenuHover = 0.0f;
}

static void DrawEndingScreen(void)
{
    // Disable input during transition
    bool inputBlocked = (transitionPhase != TRANSITION_NONE);

    const char *title = "Game Over";
    int titleFontSize = 80;
    int titleWidth = MeasureText(title, titleFontSize);
    DrawText(title, (screenWidth - titleWidth) / 2, 190, titleFontSize, MAROON);

    const char *subtitle = "The core has been destroyed.";
    int subtitleFontSize = 26;
    int subtitleWidth = MeasureText(subtitle, subtitleFontSize);
    DrawText(subtitle, (screenWidth - subtitleWidth) / 2, 190 + titleFontSize + 10, subtitleFontSize, GRAY);

    const int buttonWidth = 300;
    const int buttonHeight = 70;
    const int buttonSpacing = 30;
    const int buttonX = (screenWidth - buttonWidth) / 2;
    const int firstButtonY = 400;

    Rectangle restartButton = {(float)buttonX, (float)firstButtonY, (float)buttonWidth, (float)buttonHeight};
    Rectangle menuButton = {(float)buttonX, (float)(firstButtonY + (buttonHeight + buttonSpacing)), (float)buttonWidth, (float)buttonHeight};

    if (GuiSimpleButton(restartButton, "Restart", 32, &endingRestartHover, !inputBlocked))
    {
        ResetGame();
        StartTransition(SCREEN_GAMEPLAY); // Skip the title screen and go back to game
    }

    if (GuiSimpleButton(menuButton, "Main Menu", 32, &endingMenuHover, !inputBlocked))
    {
        ResetGame();
        titleAnimTimer = 0.0f; // Replay the title screen intro anim
        StartTransition(SCREEN_TITLE);
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
        UpdateEnemies(dt);
        UpdateCore(dt);
    }
    break;

    case SCREEN_ENDING:
    {
        // All handled in DrawEndingScreen()
    }
    break;

    case SCREEN_LOGO:
    default:
        break;
    }

    UpdateSettingsPopup(dt);
    UpdateHowToPlayPopup(dt);
    UpdateTransition(dt);
    //----------------------------------------------------------------------------------
    // Draw
    //----------------------------------------------------------------------------------
    if (currentScreen == SCREEN_TITLE)
    {
        BeginTextureMode(hexLayer);
        ClearBackground(RAYWHITE);
        DrawHexBackground();
        EndTextureMode();
    }

    BeginTextureMode(target);
    ClearBackground(RAYWHITE);

    switch (currentScreen)
    {
    case SCREEN_TITLE:
    {
        // Blurred background
        BeginShaderMode(blurShader);
        DrawTexturePro(hexLayer.texture,
                       (Rectangle){0, 0, (float)hexLayer.texture.width, -(float)hexLayer.texture.height},
                       (Rectangle){0, 0, (float)screenWidth, (float)screenHeight},
                       (Vector2){0, 0}, 0.0f, WHITE);
        EndShaderMode();

        DrawTitleScreen();
    }
    break;

    case SCREEN_GAMEPLAY:
    {
        // Popups + transition disable input on the top bar and grid
        bool inputBlocked = (transitionPhase != TRANSITION_NONE) || settingsOpen || howToPlayOpen ||
                             (popupAnim > 0.01f) || (howToPlayAnim > 0.01f);

        DrawHexGrid();
        DrawCore();
        DrawTowers();
        DrawEnemies();
        DrawInventory();
        DrawGameplayTopBar(!inputBlocked);
    }
    break;

    case SCREEN_ENDING:
    {
        DrawEndingScreen();
    }
    break;

    case SCREEN_LOGO:
    default:
        break;
    }

    DrawSettingsPopup();
    DrawHowToPlayPopup();
    DrawTransitionOverlay();

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