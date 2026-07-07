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

#include <stdio.h>  // Required for: printf()
#include <stdlib.h> // Required for:
#include <string.h> // Required for:
#include <math.h> // Required for: fminf(), fmaxf()

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
// TODO: Define global variables here, recommended to make them static

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
static void UpdateDrawFrame(void); // Update and Draw one frame

// Small math helpers
static float FLerp(float a, float b, float t);
static float FClamp(float value, float min, float max);
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

static float FLerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

static float FClamp(float value, float min, float max)
{
    if (value < min)
        return min;
    if (value > max)
        return max;
    return value;
}

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

    float scale = 1.0f + 0.04f * (*hoverAnim);
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
            (unsigned char)FLerp((float)RAYWHITE.r, (float)LIGHTGRAY.r, *hoverAnim),
            (unsigned char)FLerp((float)RAYWHITE.g, (float)LIGHTGRAY.g, *hoverAnim),
            (unsigned char)FLerp((float)RAYWHITE.b, (float)LIGHTGRAY.b, *hoverAnim),
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
    if (transitionPhase == TRANSITION_NONE) return;
 
    float alpha = 0.0f;
    if (transitionPhase == TRANSITION_OUT) alpha = FClamp(transitionTimer/TRANSITION_DURATION, 0.0f, 1.0f);
    else alpha = FClamp(1.0f - transitionTimer/TRANSITION_DURATION, 0.0f, 1.0f);
 
    DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, alpha));
}

static void UpdateTitleScreen(float dt)
{
    if (titleAnimTimer < TITLE_ANIM_DURATION)
    {
        titleAnimTimer += dt;
        if (titleAnimTimer > TITLE_ANIM_DURATION) titleAnimTimer = TITLE_ANIM_DURATION;
    }
}

static void DrawTitleScreen(void)
{
    // Title screen input is disabled while the settings popup is showing/animating
    // or while a screen transition is in progress.
    bool inputBlocked = (transitionPhase != TRANSITION_NONE) || settingsOpen || (popupAnim > 0.01f);
 
    // --- Animated, two-colored title -----------------------------------------------------
    float progress = FClamp(titleAnimTimer / TITLE_ANIM_DURATION, 0.0f, 1.0f);
    float eased = EaseOutCubic(progress);
 
    int baseFontSize = 90;
    float scale = FLerp(0.7f, 1.0f, eased);
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
 
    // --- Menu buttons ---------------------------------------------------------------------
    const int buttonWidth = 300;
    const int buttonHeight = 70;
    const int buttonSpacing = 30;
    const int buttonX = (screenWidth - buttonWidth) / 2;
    const int firstButtonY = 340;
 
    Rectangle playButton     = { (float)buttonX, (float)firstButtonY, (float)buttonWidth, (float)buttonHeight };
    Rectangle settingsButton = { (float)buttonX, (float)(firstButtonY + (buttonHeight + buttonSpacing)), (float)buttonWidth, (float)buttonHeight };
 
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
    if (popupAnim < target) popupAnim = fminf(popupAnim + popupSpeed*dt, target);
    else popupAnim = fmaxf(popupAnim - popupSpeed*dt, target);
}

static void DrawSettingsPopup(void)
{
    if (popupAnim <= 0.001f) return;
 
    float eased = EaseOutCubic(popupAnim);
 
    DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.4f*popupAnim));
 
    Rectangle basePopupRect = { screenWidth/2.0f - 200, screenHeight/2.0f - 150, 400, 300 };
 
    float scale = FLerp(0.85f, 1.0f, eased);
    float w = basePopupRect.width * scale;
    float h = basePopupRect.height * scale;
    Rectangle popupRect = {
        basePopupRect.x + (basePopupRect.width - w)/2.0f,
        basePopupRect.y + (basePopupRect.height - h)/2.0f,
        w, h
    };
 
    DrawRectangleRec(popupRect, Fade(RAYWHITE, eased));
    DrawRectangleLinesEx(popupRect, 4, Fade(BLACK, eased));
 
    const char *popupTitle = "Settings";
    int popupTitleSize = 36;
    int popupTitleWidth = MeasureText(popupTitle, popupTitleSize);
    DrawText(popupTitle, (int)(popupRect.x + (popupRect.width - popupTitleWidth) / 2.0f),
             (int)(popupRect.y + 20), popupTitleSize, Fade(BLACK, eased));
 
    Rectangle closeButton = { popupRect.x + popupRect.width/2 - 100, popupRect.y + popupRect.height - 80, 200, 50 };
    bool closeInteractive = (popupAnim > 0.95f);
    if (GuiSimpleButton(closeButton, "Close", 28, &settingsCloseHover, closeInteractive))
    {
        settingsOpen = false;
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
        } break;

        case SCREEN_GAMEPLAY:
        {
            // TODO: Gameplay 
        } break;
        
        case SCREEN_LOGO:
        case SCREEN_ENDING:
        default: break;
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
        } break;

        case SCREEN_GAMEPLAY:
        {
            // TODO: Gameplay 
        } break;
        
        case SCREEN_LOGO:
        case SCREEN_ENDING:
        default: break;
    }

    DrawSettingsPopup();

    DrawTransitionOverlay();

    DrawRectangleLinesEx((Rectangle){0, 0, screenWidth, screenHeight}, 16, BLACK);

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