#include "OSK.h"

#include <SDL/SDL.h>
#include <SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "stretch.h"
#include "Frame.h"

/* Key types */
enum
{
  OSK_NORMAL = 0,
  OSK_SPECIAL,
  OSK_APPLE,
  OSK_FUNCTION,
  OSK_SHIFT,
  OSK_CONTROL,
  OSK_CAPS
};

struct OSKKey
{
  int x;
  int y;
  int w;
  int h;

  const char *label;
  int key;
  int type;
  int row;
  int sprite;
  int spriteW;
};

static OSKKey g_keys[80];
static int g_keyCount = 0;
static int g_selected = 0;
static bool g_bOSKVisible = false;

static bool g_bOSKSelectHeld = false;

static bool g_bOSKShift = false;
static bool g_bOSKCtrl = false;
static bool g_bOSKCaps = false;

static bool g_OSKInvertJoystick = false;
static bool g_OSKSwapJoystickAxes = false;

static SDL_Joystick *g_OSKJoystick = NULL;
static bool g_OSKOwnsJoystick = false;

static int g_OSKDPadUp = -1;
static int g_OSKDPadDown = -1;
static int g_OSKDPadLeft = -1;
static int g_OSKDPadRight = -1;

static const char *OSK_SPRITE_SHEET = "res/osk.png";

static SDL_Surface *g_OSKSpriteSheet = NULL;
static SDL_Surface *g_OSKAlphaSheet = NULL;
static const Uint8 g_OSKAlphaValues[] = { 190, 140, 255 };

static const int OSK_SPRITE_COUNT = 66;
static const int OSK_SPRITE_HEIGHT = 28;

static const int OSK_GAP = 2;
static const int OSK_ROW_GAP = 2;
static const int OSK_Y_BIAS = 8;

static int g_OSKAlphaMode = 0;

/* Prevent one physical input from producing multiple OSK actions (simultaneous inputs) */
static Uint32 g_OSKLastInputTime = 0;
static const Uint32 OSK_INPUT_DELAY = 75;

/* Making VideoRedrawScreen function from Video.cpp 
 * visible and accessible here.  This is so we can
 * call it to remove the OSK from the buffer so
 * we can get a clean screenshot.
 */
void VideoRedrawScreen();

static void AddKey(int x, int y, int w, int h,
                   const char *label, int key, int type, int row)
{
  if (g_keyCount >= (int)(sizeof(g_keys) / sizeof(g_keys[0])))
    return;

  g_keys[g_keyCount].x = x;
  g_keys[g_keyCount].y = y;
  g_keys[g_keyCount].w = w;
  g_keys[g_keyCount].h = h;
  g_keys[g_keyCount].label = label;
  g_keys[g_keyCount].key = key;
  g_keys[g_keyCount].type = type;
  g_keys[g_keyCount].row = row;

  g_keys[g_keyCount].sprite = g_keyCount;

  g_keyCount++;
}

static void BuildKeyboard()
{
  if (g_keyCount)
    return;

  /* ESC  1 2 3 4 5 6 7 8 9 0 - = DELETE */
  AddKey(0, 0, 0, 0, "ESC", SDLK_ESCAPE, OSK_SPECIAL, 1);

  const char *row1[] =
  { "1","2","3","4","5","6","7","8","9","0","-","=" };

  const int row1keys[] =
  { '1','2','3','4','5','6','7','8','9','0','-','=' };

  for (int i = 0; i < 12; i++) {
    AddKey(0, 0, 0, 0, row1[i], row1keys[i], OSK_NORMAL, 1);
  }
  AddKey(0, 0, 0, 0, "DELETE", SDLK_DELETE, OSK_SPECIAL, 1);

  /* TAB Q W E R T Y U I O P [ ] \ */
  AddKey(0, 0, 0, 0, "TAB", SDLK_TAB, OSK_SPECIAL, 2);

  const char *row2[] =
  { "Q","W","E","R","T","Y","U","I","O","P","[","]","\\" };

  const int row2keys[] =
  { 'q','w','e','r','t','y','u','i','o','p','[',']','\\' };

  for (int i = 0; i < 13; i++) {
    AddKey(0, 0, 0, 0, row2[i], row2keys[i], OSK_NORMAL, 2);
  }

  /* CONTROL A S D F G H J K L ; ' RETURN */
  AddKey(0, 0, 0, 0, "CONTROL", 0, OSK_CONTROL, 3);

  const char *row3[] =
  { "A","S","D","F","G","H","J","K","L",";", "'" };

  const int row3keys[] =
  { 'a','s','d','f','g','h','j','k','l',';','\'' };

  for (int i = 0; i < 11; i++) {
    AddKey(0, 0, 0, 0, row3[i], row3keys[i], OSK_NORMAL, 3);
  }
  AddKey(0, 0, 0, 0, "RETURN", SDLK_RETURN, OSK_SPECIAL, 3);

  /* SHIFT Z X C V B N M , . / SHIFT */
  AddKey(0, 0, 0, 0, "SHIFT", 0, OSK_SHIFT, 4);

  const char *row4[] =
  { "Z","X","C","V","B","N","M",",",".","/" };

  const int row4keys[] =
  { 'z','x','c','v','b','n','m',',','.','/' };

  for (int i = 0; i < 10; i++) {
    AddKey(0, 0, 0, 0, row4[i], row4keys[i], OSK_NORMAL, 4);
  }
  AddKey(0, 0, 0, 0, "SHIFT", 0, OSK_SHIFT, 4);

  /* CAPS, `~, -gap-, Open APPLE, Space, Closed APPLE, Left, Right, Down, Up */
  AddKey(0, 0, 0, 0, "CAPS",   0,          OSK_CAPS,    5);
  AddKey(0, 0, 0, 0, "`~",     '`',        OSK_NORMAL,  5);
  AddKey(0, 0, 0, 0, "OPEN",   0,          OSK_APPLE,   5);
  AddKey(0, 0, 0, 0, "SPACE",  ' ',        OSK_NORMAL,  5);
  AddKey(0, 0, 0, 0, "CLOSED", 0,          OSK_APPLE,   5);
  AddKey(0, 0, 0, 0, "LEFT",   SDLK_LEFT,  OSK_SPECIAL, 5);
  AddKey(0, 0, 0, 0, "RIGHT",  SDLK_RIGHT, OSK_SPECIAL, 5);
  AddKey(0, 0, 0, 0, "DOWN",   SDLK_DOWN,  OSK_SPECIAL, 5);
  AddKey(0, 0, 0, 0, "UP",     SDLK_UP,    OSK_SPECIAL, 5);

  /* Emulator function buttons */
  AddKey(0, 0, 0, 0, "Swap Drives",   0, OSK_FUNCTION, 0);
  AddKey(0, 0, 0, 0, "Screenshot",    0, OSK_FUNCTION, 0);
  AddKey(0, 0, 0, 0, "Quit LinApple", 0, OSK_FUNCTION, 0);
  AddKey(0, 0, 0, 0, "OSK Alpha",     0, OSK_FUNCTION, 0);
}

static const int g_OSKSpriteWidths[OSK_SPRITE_COUNT] =
{
  28,   // ESC
  28,   // 1
  28,   // 2
  28,   // 3
  28,   // 4
  28,   // 5
  28,   // 6
  28,   // 7
  28,   // 8
  28,   // 9
  28,   // 0
  28,   // -
  28,   // =  
  46,   // DELETE
  48,   // TAB
  28,   // Q
  28,   // W
  28,   // E
  28,   // R
  28,   // T
  28,   // Y
  28,   // U
  28,   // I
  28,   // O
  28,   // P
  28,   // [
  28,   // ]
  28,   /* \ */
  56,   // CONTROL
  28,   // A
  28,   // S
  28,   // D
  28,   // F
  28,   // G
  28,   // H
  28,   // J
  28,   // K
  28,   // L
  28,   // ;
  28,   // '
  56,   // RETURN
  78,   // LEFT SHIFT
  28,   // Z
  28,   // X
  28,   // C
  28,   // V
  28,   // B
  28,   // N
  28,   // M
  28,   // ,
  28,   // .
  28,   /* / */
  78,   // RIGHT SHIFT
  28,   // CAPS LOCK
  28,   // `~
  28,   // OPEN APPLE
  206,  // SPACE
  28,   // CLOSED APPLE
  28,   // LEFT
  28,   // RIGHT
  28,   // DOWN
  28,   // UP
  102,  // Swap Drives
  102,  // Screenshot
  102,  // Quit LinApple
  102   // OSK Alpha
};

struct OSKSprite
{
  int x;
  int y;
  int w;
  int h;
};

static OSKSprite g_OSKSprites[OSK_SPRITE_COUNT];
static bool g_OSKSpriteInfoBuilt = false;


static void BuildSpriteInfo()
{
  if (g_OSKSpriteInfoBuilt) {
    return;
  }

  int x = 1;

  for (int i = 0; i < OSK_SPRITE_COUNT; i++) {
    g_OSKSprites[i].x = x;
    g_OSKSprites[i].y = 1;
    g_OSKSprites[i].w = g_OSKSpriteWidths[i];
    g_OSKSprites[i].h = OSK_SPRITE_HEIGHT;

    x += g_OSKSpriteWidths[i] + 1;
  }

  g_OSKSpriteInfoBuilt = true;
}

static void ApplyOSKAlpha()
{
  if (!g_OSKSpriteSheet) {
    return;
  }

  if (!g_OSKAlphaSheet) {
    g_OSKAlphaSheet = SDL_ConvertSurface(g_OSKSpriteSheet, g_OSKSpriteSheet->format, SDL_SWSURFACE);

    if (!g_OSKAlphaSheet) {
      fprintf(stderr, "OSK: Couldn't create alpha surface: %s\n", SDL_GetError());
      return;
    }
  }

  SDL_LockSurface(g_OSKSpriteSheet);
  SDL_LockSurface(g_OSKAlphaSheet);

  const Uint8 globalAlpha = g_OSKAlphaValues[g_OSKAlphaMode];
  const SDL_PixelFormat *format = g_OSKSpriteSheet->format;
  const int pixelCount = g_OSKSpriteSheet->w * g_OSKSpriteSheet->h;

  Uint32 *src = (Uint32 *)g_OSKSpriteSheet->pixels;
  Uint32 *dst = (Uint32 *)g_OSKAlphaSheet->pixels;

  for (int i = 0; i < pixelCount; i++) {
    Uint32 pixel = src[i];
    Uint32 alpha = (pixel & format->Amask) >> format->Ashift;
    alpha = (alpha * globalAlpha) / 255;

    dst[i] = (pixel & ~format->Amask) |
      ((alpha << format->Ashift) & format->Amask);
  }

  SDL_UnlockSurface(g_OSKAlphaSheet);
  SDL_UnlockSurface(g_OSKSpriteSheet);
}

static bool LoadOSKSpriteSheet()
{
  if (g_OSKSpriteSheet) {
    return true;
  }

  BuildSpriteInfo();
  g_OSKSpriteSheet = IMG_Load(OSK_SPRITE_SHEET);

  if (!g_OSKSpriteSheet) {
    fprintf(stderr, "OSK: Couldn't load sprite sheet %s: %s\n", OSK_SPRITE_SHEET, SDL_GetError());
    SDL_Quit();
    return false;
  }

  ApplyOSKAlpha();

  if (g_OSKSpriteSheet->w != 2583 || g_OSKSpriteSheet->h != 30) {
    fprintf(stderr, "OSK: Warning: sprite sheet is %dx%d, expected 2583x30\n",
            g_OSKSpriteSheet->w, g_OSKSpriteSheet->h);
  }

  return true;
}

static int CalculateRowWidth(int first, int last,
                             int extraGapBefore)
{
  int width = 0;

  for (int i = first; i <= last; i++) {
    width += g_OSKSprites[g_keys[i].sprite].w;

    if (i != last)
      width += OSK_GAP;

    if (i + 1 == extraGapBefore)
      width += OSK_GAP;
  }

  return width;
}

static void LayoutRow(int first, int last, int y, int screenWidth, int extraGapBefore)
{
  int width = CalculateRowWidth(first, last, extraGapBefore);
  int x = (screenWidth - width) / 2;

  for (int i = first; i <= last; i++) {
    if (i == extraGapBefore)
      x += OSK_GAP;

    g_keys[i].x = x;
    g_keys[i].y = y;
    g_keys[i].w = g_OSKSprites[g_keys[i].sprite].w;
    g_keys[i].h = g_OSKSprites[g_keys[i].sprite].h;

    x += g_keys[i].w;

    if (i != last)
      x += OSK_GAP;
  }
}

static void LayoutKeyboard(int screenWidth, int screenHeight)
{
  if (g_keyCount <= 0)
    return;

  /* All sprites are 28 pixels high */
  const int rowHeight = OSK_SPRITE_HEIGHT;

  const int totalHeight =
      rowHeight * 6 +
      OSK_ROW_GAP * 5;

  int y = (screenHeight - totalHeight) / 2 + OSK_Y_BIAS;

  /* Function buttons */
  LayoutRow(62, 65, y, screenWidth, -1);

  y += rowHeight + OSK_ROW_GAP;

  /* ESC ... DELETE */
  LayoutRow(0, 13, y, screenWidth, -1);

  y += rowHeight + OSK_ROW_GAP;

  /* TAB ... \ */
  LayoutRow(14, 27, y, screenWidth, -1);

  y += rowHeight + OSK_ROW_GAP;

  /* CONTROL ... RETURN */
  LayoutRow(28, 40, y, screenWidth, -1);

  y += rowHeight + OSK_ROW_GAP;

  /* LSHIFT ... RSHIFT */
  LayoutRow(41, 52, y, screenWidth, -1);

  y += rowHeight + OSK_ROW_GAP;

  /* CAPS ... UP */
  LayoutRow(53, 61, y, screenWidth, 55);
}

/* Function for mouse OSK interaction.  May not use this,
 * but more work is needed to add mouse support for the OSK.
 */
//static bool PointInKey(const OSKKey &key, int x, int y)
//{
//  return x >= key.x &&
//         x < key.x + key.w &&
//         y >= key.y &&
//         y < key.y + key.h;
//}

static bool OSK_AllowInput()
{
  Uint32 now = SDL_GetTicks();

  if ((now - g_OSKLastInputTime) < OSK_INPUT_DELAY) {
    return false;
  }

  g_OSKLastInputTime = now;
  return true;
}

/* Move selection to the nearest key in the requested direction. */
static void MoveHorizontal(int direction)
{
  if (g_keyCount <= 0) {
    return;
  }

  const int row = g_keys[g_selected].row;
  int best = -1;

  if (direction < 0) {
    for (int i = g_selected - 1; i >= 0; i--)
    {
      if (g_keys[i].row == row) {
        best = i;
        break;
      }
    }
  }
  else
  {
    for (int i = g_selected + 1; i < g_keyCount; i++)
    {
      if (g_keys[i].row == row) {
        best = i;
        break;
      }
    }
  }

  if (best >= 0) {
    g_selected = best;
  }
}

static void MoveVertical(int direction)
{
  if (g_keyCount <= 0) {
    return;
  }

  const int currentRow = g_keys[g_selected].row;
  const int targetRow = currentRow + direction;

  if (targetRow < 0) {
    return;
  }

  int currentCenter = g_keys[g_selected].x + g_keys[g_selected].w / 2;

  int best = -1;
  int bestDistance = 0x7fffffff;

  for (int i = 0; i < g_keyCount; i++)
  {
    if (g_keys[i].row != targetRow) {
      continue;
    }

    int candidateCenter = g_keys[i].x + g_keys[i].w / 2;
    int distance = candidateCenter - currentCenter;

    if (distance < 0) {
      distance = -distance;
    }

    if (distance < bestDistance) {
      bestDistance = distance;
      best = i;
    }
  }

  if (best >= 0) {
    g_selected = best;
  }
}

static void OSK_OpenJoystick()
{
  g_OSKJoystick = NULL;
  g_OSKOwnsJoystick = false;

  /* If LinApple is already using joystick 0, use its existing SDL joystick handle. Do not open a second handle. */
  if (JoyIsJoystick0Enabled()) {
    g_OSKJoystick = JoyGetJoystick0();
    return;
  }

  /* LinApple has joysticks disabled. Open the configured joystick temporarily for OSK use only. */
  if (SDL_NumJoysticks() > 0) {
    g_OSKJoystick = SDL_JoystickOpen(JoyGetJoystick0Index());

    if (g_OSKJoystick) {
      g_OSKOwnsJoystick = true;
    }
  }
}

static void OSK_CloseJoystick()
{
  if (g_OSKOwnsJoystick && g_OSKJoystick) {
    SDL_JoystickClose(g_OSKJoystick);
  }
  g_OSKJoystick = NULL;
  g_OSKOwnsJoystick = false;
}

static void ActivateSelected()
{
  if (g_keyCount <= 0) {
    return;
  }

  OSKKey &key = g_keys[g_selected];

  if (key.type == OSK_SHIFT) {
      g_bOSKShift = !g_bOSKShift;
      return;
  }
  
  if (key.type == OSK_CONTROL) {
      g_bOSKCtrl = !g_bOSKCtrl;
      return;
  }
  
  if (key.type == OSK_CAPS) {
      g_bOSKCaps = !g_bOSKCaps;
      return;
  }

  if (key.type == OSK_FUNCTION) {
    SDL_Event event;

    if (key.label && !strcmp(key.label, "Swap Drives"))
    {
      g_bOSKVisible = false;
      OSK_CloseJoystick();
      SDL_Delay(20);
      memset(&event, 0, sizeof(event));

      event.type = SDL_KEYDOWN;
      event.key.type = SDL_KEYDOWN;
      event.key.state = SDL_PRESSED;
      event.key.keysym.sym = SDLK_F5;
      SDL_PushEvent(&event);

      memset(&event, 0, sizeof(event));

      event.type = SDL_KEYUP;
      event.key.type = SDL_KEYUP;
      event.key.state = SDL_RELEASED;
      event.key.keysym.sym = SDLK_F5;
      SDL_PushEvent(&event);
    }
    else if (key.label && !strcmp(key.label, "Screenshot"))
    {
      g_bOSKVisible = false;
      OSK_CloseJoystick();
      VideoRedrawScreen();
      SDL_Delay(20);
      memset(&event, 0, sizeof(event));

      event.type = SDL_KEYDOWN;
      event.key.type = SDL_KEYDOWN;
      event.key.state = SDL_PRESSED;
      event.key.keysym.sym = SDLK_F8;
      SDL_PushEvent(&event);

      memset(&event, 0, sizeof(event));

      event.type = SDL_KEYUP;
      event.key.type = SDL_KEYUP;
      event.key.state = SDL_RELEASED;
      event.key.keysym.sym = SDLK_F8;
      SDL_PushEvent(&event);
    }
    else if (key.label && !strcmp(key.label, "Quit LinApple"))
    {
      event.type = SDL_QUIT;
      SDL_PushEvent(&event);            
    }
    else if (key.label && !strcmp(key.label, "OSK Alpha"))
    {
      g_OSKAlphaMode++;

      if (g_OSKAlphaMode >= 3)
        g_OSKAlphaMode = 0;

      ApplyOSKAlpha();
    }

    return;
  }


  if (key.type == OSK_APPLE) {
    SDL_Event event;

    if (key.label && !strcmp(key.label, "OPEN"))
    {
      memset(&event, 0, sizeof(event));

      event.type = SDL_KEYDOWN;
      event.key.type = SDL_KEYDOWN;
      event.key.state = SDL_PRESSED;
      event.key.keysym.sym = SDLK_LALT;
      SDL_PushEvent(&event);

      memset(&event, 0, sizeof(event));

      event.type = SDL_KEYUP;
      event.key.type = SDL_KEYUP;
      event.key.state = SDL_RELEASED;
      event.key.keysym.sym = SDLK_LALT;
      SDL_PushEvent(&event);
    }
    else if (key.label && !strcmp(key.label, "CLOSED"))
    {
      memset(&event, 0, sizeof(event));

      event.type = SDL_KEYDOWN;
      event.key.type = SDL_KEYDOWN;
      event.key.state = SDL_PRESSED;
      event.key.keysym.sym = SDLK_RALT;
      SDL_PushEvent(&event);

      memset(&event, 0, sizeof(event));

      event.type = SDL_KEYUP;
      event.key.type = SDL_KEYUP;
      event.key.state = SDL_RELEASED;
      event.key.keysym.sym = SDLK_RALT;
      SDL_PushEvent(&event);
    }

    return;
  }


  if (key.key == 0) {
    if (!key.label)
      return;

    if (!strcmp(key.label, "SHIFT"))
      g_bOSKShift = !g_bOSKShift;
    else if (!strcmp(key.label, "CONTROL"))
      g_bOSKCtrl = !g_bOSKCtrl;
    else if (!strcmp(key.label, "CAPS"))
      g_bOSKCaps = !g_bOSKCaps;

    return;
  }

  /* KeybQueueKeypress() supplied by LinApple's existing keyboard implementation. */
  extern void KeybQueueKeypress(int key, bool bASCII);

  int keycode = key.key;

  /* Virtual Caps/Shift */
  if (g_bOSKShift) {
    if (keycode >= 'a' && keycode <= 'z') {
      keycode -= ('a' - 'A');
    }
    else
    {
      switch (keycode)
      {
        case '1': keycode = '!'; break;
        case '2': keycode = '@'; break;
        case '3': keycode = '#'; break;
        case '4': keycode = '$'; break;
        case '5': keycode = '%'; break;
        case '6': keycode = '^'; break;
        case '7': keycode = '&'; break;
        case '8': keycode = '*'; break;
        case '9': keycode = '('; break;
        case '0': keycode = ')'; break;
        case '-': keycode = '_'; break;
        case '=': keycode = '+'; break;
        case '[': keycode = '{'; break;
        case ']': keycode = '}'; break;
        case '\\': keycode = '|'; break;
        case ';': keycode = ':'; break;
        case '\'': keycode = '"'; break;
        case ',': keycode = '<'; break;
        case '.': keycode = '>'; break;
        case '/': keycode = '?'; break;
        case '`': keycode = '~'; break;
      }
    }
  }

  if (g_bOSKCtrl &&
      keycode >= 'a' &&
      keycode <= 'z')
  {
    keycode = keycode - 'a' + 1;
  }
  else if (g_bOSKCtrl &&
           keycode >= 'A' &&
           keycode <= 'Z')
  {
    keycode = keycode - 'A' + 1;
  }

  KeybQueueKeypress(keycode, true);

  /* Shift and Control behave like modifiers for one subsequent ordinary key. */
  g_bOSKShift = false;
  g_bOSKCtrl = false;
}

bool OSK_IsVisible()
{
    return g_bOSKVisible;
}

void OSK_Show()
{
    BuildKeyboard();
    LoadOSKSpriteSheet();

    g_bOSKVisible = true;

    g_selected = 0;

    g_bOSKShift = false;
    g_bOSKCtrl = false;

    g_OSKLastInputTime = 0;
    g_bOSKSelectHeld = true;

    g_OSKInvertJoystick = false;
    g_OSKSwapJoystickAxes = false;

    /* The DEVICE_NAME envar is expected to be populated/filled in by PortMaster on supported devices */
    const char *deviceName = getenv("DEVICE_NAME");

    if (deviceName &&
        (strcmp(deviceName, "RG351P") == 0 ||
         strcmp(deviceName, "RG351M") == 0 ||
         strcmp(deviceName, "RG351V") == 0))
    {
    // OpenSimHardware OSH PB Controller
        g_OSKInvertJoystick = true;
        /*  We won't set the D-Pad buttons here since the D-Pad is a POV HAT and not 
         *  buttons-by-number.  Leaving these set to -1 can prevent some really goofy
         *  and unwanted inputs.
         */
    }
    else if (access("/dev/input/by-path/platform-ff300000.usb-usb-0:1.2:1.0-event-joystick", F_OK) == 0) {
    // OpenSimHardware OSH PB Controller
        g_OSKInvertJoystick = true;
        /* Same as above */
    }
    else if (deviceName && strcmp(deviceName, "GameForce") == 0) {
    // Gameforce Chi - gameforce_gamepad
        g_OSKSwapJoystickAxes = true;
        g_OSKDPadUp = 10;
        g_OSKDPadDown = 11;
        g_OSKDPadLeft = 12;
        g_OSKDPadRight = 13;
    }
    else if (access("/dev/input/by-path/platform-gameforce-gamepad-event-joystick", F_OK) == 0) {
    // Gameforce Chi - gameforce_gamepad
        g_OSKSwapJoystickAxes = true;
        g_OSKDPadUp = 10;
        g_OSKDPadDown = 11;
        g_OSKDPadLeft = 12;
        g_OSKDPadRight = 13;
    }
    else if (deviceName &&
        (strcmp(deviceName, "RG552") == 0 ||
         strcmp(deviceName, "RG503") == 0 ||
         strcmp(deviceName, "x55") == 0 ||
         strcmp(deviceName, "Powkiddy x55") == 0 ||
         strcmp(deviceName, "RGB30") == 0 ||
         strcmp(deviceName, "RG353PS") == 0 ||
         strcmp(deviceName, "RG353V") == 0 ||
         strcmp(deviceName, "RG353VS") == 0 ||
         strcmp(deviceName, "RG353M") == 0 ||
         strcmp(deviceName, "RG353P") == 0))
    {
    // retrogame_joypad
    // zed_joystick (Powkiddy x55) (shares the same D-Pad button layout as the retrogame_joypad)
        g_OSKDPadUp = 13;
        g_OSKDPadDown = 14;
        g_OSKDPadLeft = 15;
        g_OSKDPadRight = 16;
    }
    else if (access("/dev/input/by-path/platform-singleadc-joypad-event-joystick", F_OK) == 0) {
    // retrogame_joypad
        g_OSKDPadUp = 13;
        g_OSKDPadDown = 14;
        g_OSKDPadLeft = 15;
        g_OSKDPadRight = 16;
    }
    else if (deviceName &&
        (strcmp(deviceName, "ODROID-GO Super") == 0 ||
         strcmp(deviceName, "RG351MP") == 0 ||
         strcmp(deviceName, "RGB10") == 0))
    {
    // GO-Super Gamepad
        g_OSKDPadUp = 8;
        g_OSKDPadDown = 9;
        g_OSKDPadLeft = 10;
        g_OSKDPadRight = 11;
    }
    else if (access("/dev/input/by-path/platform-odroidgo3-joypad-event-joystick", F_OK) == 0) {
    // GO-Super Gamepad
        g_OSKDPadUp = 8;
        g_OSKDPadDown = 9;
        g_OSKDPadLeft = 10;
        g_OSKDPadRight = 11;
    }
    else if (deviceName &&
        (strcmp(deviceName, "RG35XX") == 0 ||
         strcmp(deviceName, "RG35XX-H") == 0 ||
         strcmp(deviceName, "RG40XX") == 0 ||
         strcmp(deviceName, "RG40XX-H") == 0 ||
         strcmp(deviceName, "RG34XX-SP") == 0))
    {
    // gpio_keys
        g_OSKDPadUp = 13;
        g_OSKDPadDown = 14;
        g_OSKDPadLeft = 15;
        g_OSKDPadRight = 16;
    }
    else if (access("/dev/input/by-path/platform-soc@03000000:gpio_keys-event-joystick", F_OK) == 0) {
    // gpio_keys
      // Controller device path for the RG34XX, RG35XX, and RG40XX devices.
      // Evidently this is also the case for the Retroid Pocket Nova.
        g_OSKDPadUp = 13;
        g_OSKDPadDown = 14;
        g_OSKDPadLeft = 15;
        g_OSKDPadRight = 16;
    }
    else if (access("/dev/input/by-path/platform-odroidgo2-joypad-event-joystick", F_OK) == 0) {
    // GO-Advance Gamepad (rev 1.1)
        // For the older RGB10 and the Odroid-Go Advance
        g_OSKDPadUp = 8;
        g_OSKDPadDown = 9;
        g_OSKDPadLeft = 10;
        g_OSKDPadRight = 11;
    }
    else if (deviceName && strcmp(deviceName, "RK2020") == 0) {
    // GO-Advance Gamepad (rev 1.0)
        // This uses the same device path as the old RGB10 and the OGA, but...
        g_OSKDPadUp = 6;
        g_OSKDPadDown = 7;
        g_OSKDPadLeft = 8;
        g_OSKDPadRight = 9;
    }

    /* Function to enable joystick for OSK if joystick support is disabled in LinApple */
    OSK_OpenJoystick();
}

void OSK_Toggle()
{
  if (g_bOSKVisible) {
    g_bOSKVisible = false;
    OSK_CloseJoystick();
  } else {
    OSK_Show();
  }
}

bool OSK_HandleEvent(SDL_Event *event)
{
  if (!event) {
    return false;
  }

  if (!g_bOSKVisible) {
    return false;
  }

  switch (event->type)
  {
    case SDL_KEYDOWN:
    {
      const SDLKey sym = event->key.keysym.sym;

      /* Physical Escape closes the OSK.  Invoking 'Esc' from the OSK sends it to the emulator. */
      if (sym == SDLK_ESCAPE) {
        g_bOSKVisible = false;
        OSK_CloseJoystick();
        return true;
      }
      if (sym == SDLK_UP || sym == SDLK_KP8 || sym == SDLK_e || sym == SDLK_u || sym == SDLK_w || sym == SDLK_y) {
        if (OSK_AllowInput())
          MoveVertical(-1);
        return true;
      }
      if (sym == SDLK_DOWN || sym == SDLK_KP2 || sym == SDLK_c || sym == SDLK_m || sym == SDLK_v) {
        if (OSK_AllowInput())
          MoveVertical(1);
        return true;
      }
      if (sym == SDLK_LEFT || sym == SDLK_KP4 || sym == SDLK_t) {
        if (OSK_AllowInput())
          MoveHorizontal(-1);
        return true;
      }
      if (sym == SDLK_RIGHT || sym == SDLK_KP6 || sym == SDLK_p) {
        if (OSK_AllowInput())
          MoveHorizontal(1);
        return true;
      }
      if (sym == SDLK_SPACE || sym == SDLK_RETURN || sym == SDLK_KP_ENTER) {
        if (OSK_AllowInput())
          ActivateSelected();
        return true;
      }

      return true;
    }

    case SDL_JOYBUTTONDOWN:
    {
      if (event->jbutton.button == 0 || event->jbutton.button == 1 || 
          event->jbutton.button == 2 || event->jbutton.button == 3 || 
          event->jbutton.button == 4)
      {
        if (OSK_AllowInput())
          ActivateSelected();
        return true;
      }

      if (event->jbutton.button == g_OSKDPadUp) {
        if (OSK_AllowInput())
          MoveVertical(-1);
        return true;
      }
      if (event->jbutton.button == g_OSKDPadDown) {
        if (OSK_AllowInput())
          MoveVertical(1);
        return true;
      }
      if (event->jbutton.button == g_OSKDPadLeft) {
        if (OSK_AllowInput())
          MoveHorizontal(-1);
        return true;
      }
      if (event->jbutton.button == g_OSKDPadRight) {
        if (OSK_AllowInput())
          MoveHorizontal(1);
        return true;
      }

      return true;
    }

    case SDL_JOYHATMOTION:
    {
      switch (event->jhat.value)
        {
          case SDL_HAT_UP:
            if (OSK_AllowInput())
              MoveVertical(-1);
            break;
          case SDL_HAT_DOWN:
            if (OSK_AllowInput())
              MoveVertical(1);
            break;
          case SDL_HAT_LEFT:
            if (OSK_AllowInput())
              MoveHorizontal(-1);
            break;
          case SDL_HAT_RIGHT:
            if (OSK_AllowInput())
              MoveHorizontal(1);
            break;

          default:
            break;
        }

      return true;
    }

    case SDL_JOYAXISMOTION:
    {
        Sint16 value = event->jaxis.value;
        int axis = event->jaxis.axis;

        if (g_OSKSwapJoystickAxes) {
            axis = 1 - axis;
        }
        if (g_OSKInvertJoystick) {
            value = -value;
        }

        if (axis == 0)      {
        if (value < -16000)
        {
          if (OSK_AllowInput())
            MoveHorizontal(-1);
        }
        else if (value > 16000)
        {
          if (OSK_AllowInput())
            MoveHorizontal(1);
        }
      }
      else if (axis == 1)
      {
        if (value < -16000)
        {
          if (OSK_AllowInput())
            MoveVertical(-1);
        }
        else if (value > 16000)
        {
          if (OSK_AllowInput())
            MoveVertical(1);
        }
      }

      return true;
    }

    default:
      return true;
  }
}

void OSK_Draw(SDL_Surface *surface) {
  if (!g_bOSKVisible || !surface) {
    return;
  }

  BuildKeyboard();

  if (!LoadOSKSpriteSheet()) {
    return;
  }

  LayoutKeyboard(surface->w, surface->h);

  for (int i = 0; i < g_keyCount; i++) {
    const OSKKey &key = g_keys[i];

    if (key.sprite < 0 || key.sprite >= OSK_SPRITE_COUNT) {
      continue;
    }

    SDL_Rect src;
      src.x = g_OSKSprites[key.sprite].x;
      src.y = g_OSKSprites[key.sprite].y;
      src.w = g_OSKSprites[key.sprite].w;
      src.h = g_OSKSprites[key.sprite].h;

    SDL_Rect dst;
      dst.w = key.w;
      dst.h = key.h;
      dst.x = key.x;
      dst.y = key.y;

    SDL_BlitSurface(g_OSKAlphaSheet, &src, surface, &dst);
  }

  if (g_selected >= 0 && g_selected < g_keyCount) {
    const OSKKey &key = g_keys[g_selected];

    rectangle(surface,
              key.x,
              key.y,
              key.w,
              key.h,
              SDL_MapRGB(surface->format, 240, 240, 240));

    rectangle(surface,
              key.x + 1,
              key.y + 1,
              key.w - 2,
              key.h - 2,
              SDL_MapRGB(surface->format, 240, 240, 240));
  }
  
  for (int i = 0; i < g_keyCount; i++) {
    const OSKKey &key = g_keys[i];
  
    if ((key.type == OSK_SHIFT && g_bOSKShift) ||
        (key.type == OSK_CONTROL && g_bOSKCtrl) ||
        (key.type == OSK_CAPS && g_bOSKCaps))
    {
      rectangle(surface,
                key.x,
                key.y,
                key.w,
                key.h,
                SDL_MapRGB(surface->format, 80, 160, 255));

      rectangle(surface,
                key.x + 1,
                key.y + 1,
                key.w - 2,
                key.h - 2,
                SDL_MapRGB(surface->format, 80, 160, 255));
    }
  }
}

void OSK_Cleanup() {
  OSK_CloseJoystick();

  if (g_OSKSpriteSheet) {
    SDL_FreeSurface(g_OSKSpriteSheet);
  }
  g_OSKSpriteSheet = NULL;

  if (g_OSKAlphaSheet) {
    SDL_FreeSurface(g_OSKAlphaSheet);
  }
  g_OSKAlphaSheet = NULL;
}