/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2007, Tom Charlesworth, Michael Pohoreski

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Description: Frame
 *
 * Author: Various
 */

/* Adaptation for SDL and POSIX (l) by beom beotiger, Nov-Dec 2007 */

/* And KREZ */

#include <iostream>
// for stat in FrameSaveBMP function
#include <sys/stat.h>
// for usleep
#include <unistd.h>

#ifdef SDL2
#include <SDL2/SDL_opengles2.h>
// for embedded XPMs
#include <SDL2/SDL_image.h>
#else
// for embedded XPMs
#include <SDL/SDL_image.h>
#endif

#include "stdafx.h"
#include "asset.h"
#include "MouseInterface.h"

// On-screen keyboard
#include "OSK.h"

#define ENABLE_MENU 0

#ifdef SDL2
SDL_Window *sdl2window;

static SDL_GLContext sdl2glcontext = NULL;
static GLuint gles_program = 0;

static GLuint gles_textures[2] = { 0, 0 };
static int gles_texture_width[2] = { 0, 0 };
static int gles_texture_height[2] = { 0, 0 };
static unsigned int gles_texture_index = 0;

static GLint gles_position = -1;
static GLint gles_texcoord = -1;
static GLint gles_sampler = -1;

// Resolve GLES entry points through SDL so they belong to the active
// SDL-created KMSDRM/EGL context, rather than the directly linked
// GLES library variant.
static PFNGLCREATESHADERPROC p_glCreateShader = NULL;
static PFNGLSHADERSOURCEPROC p_glShaderSource = NULL;
static PFNGLCOMPILESHADERPROC p_glCompileShader = NULL;
static PFNGLGETSHADERIVPROC p_glGetShaderiv = NULL;
static PFNGLGETSHADERINFOLOGPROC p_glGetShaderInfoLog = NULL;
static PFNGLGETERRORPROC p_glGetError = NULL;
static PFNGLDELETESHADERPROC p_glDeleteShader = NULL;
static PFNGLCREATEPROGRAMPROC p_glCreateProgram = NULL;
static PFNGLATTACHSHADERPROC p_glAttachShader = NULL;
static PFNGLBINDATTRIBLOCATIONPROC p_glBindAttribLocation = NULL;
static PFNGLLINKPROGRAMPROC p_glLinkProgram = NULL;
static PFNGLDELETEPROGRAMPROC p_glDeleteProgram = NULL;
static PFNGLGETPROGRAMIVPROC p_glGetProgramiv = NULL;
static PFNGLGETPROGRAMINFOLOGPROC p_glGetProgramInfoLog = NULL;
static PFNGLGETATTRIBLOCATIONPROC p_glGetAttribLocation = NULL;
static PFNGLGETUNIFORMLOCATIONPROC p_glGetUniformLocation = NULL;
static PFNGLGENTEXTURESPROC p_glGenTextures = NULL;
static PFNGLBINDTEXTUREPROC p_glBindTexture = NULL;
static PFNGLTEXPARAMETERIPROC p_glTexParameteri = NULL;
static PFNGLPIXELSTOREIPROC p_glPixelStorei = NULL;
static PFNGLDISABLEPROC p_glDisable = NULL;
static PFNGLGETSTRINGPROC p_glGetString = NULL;
static PFNGLVIEWPORTPROC p_glViewport = NULL;
static PFNGLCLEARCOLORPROC p_glClearColor = NULL;
static PFNGLCLEARPROC p_glClear = NULL;
static PFNGLACTIVETEXTUREPROC p_glActiveTexture = NULL;
static PFNGLTEXIMAGE2DPROC p_glTexImage2D = NULL;
static PFNGLTEXSUBIMAGE2DPROC p_glTexSubImage2D = NULL;
static PFNGLUSEPROGRAMPROC p_glUseProgram = NULL;
static PFNGLUNIFORM1IPROC p_glUniform1i = NULL;
static PFNGLVERTEXATTRIBPOINTERPROC p_glVertexAttribPointer = NULL;
static PFNGLENABLEVERTEXATTRIBARRAYPROC p_glEnableVertexAttribArray = NULL;
static PFNGLDRAWARRAYSPROC p_glDrawArrays = NULL;
static PFNGLDISABLEVERTEXATTRIBARRAYPROC p_glDisableVertexAttribArray = NULL;
static PFNGLDELETETEXTURESPROC p_glDeleteTextures = NULL;

static bool GLESLoadFunctions(void)
{
#define LOAD_GLES_PROC(variable, type, name) \
    variable = (type)SDL_GL_GetProcAddress(name); \
    if (!variable) { \
      fprintf(stderr, "Missing GLES function: %s\n", name); \
      return false; \
    }

  LOAD_GLES_PROC(p_glCreateShader, PFNGLCREATESHADERPROC, "glCreateShader")
  LOAD_GLES_PROC(p_glShaderSource, PFNGLSHADERSOURCEPROC, "glShaderSource")
  LOAD_GLES_PROC(p_glCompileShader, PFNGLCOMPILESHADERPROC, "glCompileShader")
  LOAD_GLES_PROC(p_glGetShaderiv, PFNGLGETSHADERIVPROC, "glGetShaderiv")
  LOAD_GLES_PROC(p_glGetShaderInfoLog, PFNGLGETSHADERINFOLOGPROC, "glGetShaderInfoLog")
  LOAD_GLES_PROC(p_glGetError, PFNGLGETERRORPROC, "glGetError")
  LOAD_GLES_PROC(p_glDeleteShader, PFNGLDELETESHADERPROC, "glDeleteShader")
  LOAD_GLES_PROC(p_glCreateProgram, PFNGLCREATEPROGRAMPROC, "glCreateProgram")
  LOAD_GLES_PROC(p_glAttachShader, PFNGLATTACHSHADERPROC, "glAttachShader")
  LOAD_GLES_PROC(p_glBindAttribLocation, PFNGLBINDATTRIBLOCATIONPROC, "glBindAttribLocation")
  LOAD_GLES_PROC(p_glLinkProgram, PFNGLLINKPROGRAMPROC, "glLinkProgram")
  LOAD_GLES_PROC(p_glDeleteProgram, PFNGLDELETEPROGRAMPROC, "glDeleteProgram")
  LOAD_GLES_PROC(p_glGetProgramiv, PFNGLGETPROGRAMIVPROC, "glGetProgramiv")
  LOAD_GLES_PROC(p_glGetProgramInfoLog, PFNGLGETPROGRAMINFOLOGPROC, "glGetProgramInfoLog")
  LOAD_GLES_PROC(p_glGetAttribLocation, PFNGLGETATTRIBLOCATIONPROC, "glGetAttribLocation")
  LOAD_GLES_PROC(p_glGetUniformLocation, PFNGLGETUNIFORMLOCATIONPROC, "glGetUniformLocation")
  LOAD_GLES_PROC(p_glGenTextures, PFNGLGENTEXTURESPROC, "glGenTextures")
  LOAD_GLES_PROC(p_glBindTexture, PFNGLBINDTEXTUREPROC, "glBindTexture")
  LOAD_GLES_PROC(p_glTexParameteri, PFNGLTEXPARAMETERIPROC, "glTexParameteri")
  LOAD_GLES_PROC(p_glPixelStorei, PFNGLPIXELSTOREIPROC, "glPixelStorei")
  LOAD_GLES_PROC(p_glDisable, PFNGLDISABLEPROC, "glDisable")
  LOAD_GLES_PROC(p_glGetString, PFNGLGETSTRINGPROC, "glGetString")
  LOAD_GLES_PROC(p_glViewport, PFNGLVIEWPORTPROC, "glViewport")
  LOAD_GLES_PROC(p_glClearColor, PFNGLCLEARCOLORPROC, "glClearColor")
  LOAD_GLES_PROC(p_glClear, PFNGLCLEARPROC, "glClear")
  LOAD_GLES_PROC(p_glActiveTexture, PFNGLACTIVETEXTUREPROC, "glActiveTexture")
  LOAD_GLES_PROC(p_glTexImage2D, PFNGLTEXIMAGE2DPROC, "glTexImage2D")
  LOAD_GLES_PROC(p_glTexSubImage2D, PFNGLTEXSUBIMAGE2DPROC, "glTexSubImage2D")
  LOAD_GLES_PROC(p_glUseProgram, PFNGLUSEPROGRAMPROC, "glUseProgram")
  LOAD_GLES_PROC(p_glUniform1i, PFNGLUNIFORM1IPROC, "glUniform1i")
  LOAD_GLES_PROC(p_glVertexAttribPointer, PFNGLVERTEXATTRIBPOINTERPROC, "glVertexAttribPointer")
  LOAD_GLES_PROC(p_glEnableVertexAttribArray, PFNGLENABLEVERTEXATTRIBARRAYPROC, "glEnableVertexAttribArray")
  LOAD_GLES_PROC(p_glDrawArrays, PFNGLDRAWARRAYSPROC, "glDrawArrays")
  LOAD_GLES_PROC(p_glDisableVertexAttribArray, PFNGLDISABLEVERTEXATTRIBARRAYPROC, "glDisableVertexAttribArray")
  LOAD_GLES_PROC(p_glDeleteTextures, PFNGLDELETETEXTURESPROC, "glDeleteTextures")

#undef LOAD_GLES_PROC
  return true;
}

// Redirect GLES calls to SDL's context-resolved entry points.
#define glCreateShader p_glCreateShader
#define glShaderSource p_glShaderSource
#define glCompileShader p_glCompileShader
#define glGetShaderiv p_glGetShaderiv
#define glGetShaderInfoLog p_glGetShaderInfoLog
#define glGetError p_glGetError
#define glDeleteShader p_glDeleteShader
#define glCreateProgram p_glCreateProgram
#define glAttachShader p_glAttachShader
#define glBindAttribLocation p_glBindAttribLocation
#define glLinkProgram p_glLinkProgram
#define glDeleteProgram p_glDeleteProgram
#define glGetProgramiv p_glGetProgramiv
#define glGetProgramInfoLog p_glGetProgramInfoLog
#define glGetAttribLocation p_glGetAttribLocation
#define glGetUniformLocation p_glGetUniformLocation
#define glGenTextures p_glGenTextures
#define glBindTexture p_glBindTexture
#define glTexParameteri p_glTexParameteri
#define glPixelStorei p_glPixelStorei
#define glDisable p_glDisable
#define glGetString p_glGetString
#define glViewport p_glViewport
#define glClearColor p_glClearColor
#define glClear p_glClear
#define glActiveTexture p_glActiveTexture
#define glTexImage2D p_glTexImage2D
#define glTexSubImage2D p_glTexSubImage2D
#define glUseProgram p_glUseProgram
#define glUniform1i p_glUniform1i
#define glVertexAttribPointer p_glVertexAttribPointer
#define glEnableVertexAttribArray p_glEnableVertexAttribArray
#define glDrawArrays p_glDrawArrays
#define glDisableVertexAttribArray p_glDisableVertexAttribArray
#define glDeleteTextures p_glDeleteTextures

static GLuint GLESCompileShader(GLenum type, const char *source)
{
  GLuint shader = glCreateShader(type);
  if (!shader) {
    fprintf(stderr, "GLES glCreateShader failed\n");
    return 0;
  }

  glShaderSource(shader, 1, &source, NULL);
  glCompileShader(shader);

  GLint ok = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);

  if (!ok) {
    GLint logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

    char log[2048] = {0};
    GLsizei written = 0;
    if (logLength > 1) {
      glGetShaderInfoLog(shader, sizeof(log) - 1, &written, log);
    }

    fprintf(stderr, "GLES %s shader compilation failed: %s\n",
            type == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
    fprintf(stderr, "Shader source:\n%s\n", source);

    glDeleteShader(shader);
    return 0;
  }

  return shader;
}

static bool GLESInitialize(void)
{
  if (!GLESLoadFunctions()) {
    return false;
  }

  const char *vertex_source =
    "attribute vec2 a_position;\n"
    "attribute vec2 a_texcoord;\n"
    "varying vec2 v_texcoord;\n"
    "void main() { gl_Position = vec4(a_position, 0.0, 1.0); v_texcoord = a_texcoord; }\n";

  const char *fragment_source =
    "precision mediump float;\n"
    "varying vec2 v_texcoord;\n"
    "uniform sampler2D u_texture;\n"
    "void main() { gl_FragColor = texture2D(u_texture, v_texcoord); }\n";

  GLuint vs = GLESCompileShader(GL_VERTEX_SHADER, vertex_source);
  GLuint fs = GLESCompileShader(GL_FRAGMENT_SHADER, fragment_source);

  if (!vs || !fs) {
    if (vs) { glDeleteShader(vs); }
    if (fs) { glDeleteShader(fs); }
    return false;
  }

  gles_program = glCreateProgram();
  if (!gles_program) {
    glDeleteShader(vs);
    glDeleteShader(fs);
    return false;
  }

  glAttachShader(gles_program, vs);
  glAttachShader(gles_program, fs);
  glBindAttribLocation(gles_program, 0, "a_position");
  glBindAttribLocation(gles_program, 1, "a_texcoord");
  glLinkProgram(gles_program);
  glDeleteShader(vs);
  glDeleteShader(fs);

  GLint linked = GL_FALSE;
  glGetProgramiv(gles_program, GL_LINK_STATUS, &linked);
  if (!linked) {
    char log[1024] = {0};
    glGetProgramInfoLog(gles_program, sizeof(log), NULL, log);
    fprintf(stderr, "GLES program link failed: %s\n", log);
    glDeleteProgram(gles_program);
    gles_program = 0;
    return false;
  }

  gles_position = glGetAttribLocation(gles_program, "a_position");
  gles_texcoord = glGetAttribLocation(gles_program, "a_texcoord");
  gles_sampler = glGetUniformLocation(gles_program, "u_texture");

  glGenTextures(2, gles_textures);

  for (int i = 0; i < 2; ++i) {
    glBindTexture(GL_TEXTURE_2D, gles_textures[i]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }

  gles_texture_width[0] = gles_texture_width[1] = 0;
  gles_texture_height[0] = gles_texture_height[1] = 0;
  gles_texture_index = 0;

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);

  printf("OpenGL ES renderer: %s\n", (const char *)glGetString(GL_RENDERER));
  printf("OpenGL ES version: %s\n", (const char *)glGetString(GL_VERSION));
  return true;
}

void FrameShutdownGL(void)
{
  if (sdl2glcontext && sdl2window) {
    SDL_GL_MakeCurrent(sdl2window, sdl2glcontext);

    glDeleteTextures(2, gles_textures);
    gles_textures[0] = 0;
    gles_textures[1] = 0;
    gles_texture_width[0] = gles_texture_width[1] = 0;
    gles_texture_height[0] = gles_texture_height[1] = 0;

    if (gles_program) glDeleteProgram(gles_program);

    gles_textures[0] = 0;
    gles_textures[1] = 0;
    gles_texture_width[0] = gles_texture_width[1] = 0;
    gles_texture_height[0] = gles_texture_height[1] = 0;

    gles_program = 0;

    SDL_GL_MakeCurrent(sdl2window, NULL);
    SDL_GL_DeleteContext(sdl2glcontext);
    sdl2glcontext = NULL;
  }
}

void FramePresentGL(SDL_Surface *frame)
{
  if (!sdl2window || !sdl2glcontext || !gles_program ||
      !gles_textures[0] || !gles_textures[1] || !frame) {
    return;
  }

  if (SDL_GL_GetCurrentContext() != sdl2glcontext &&
      SDL_GL_MakeCurrent(sdl2window, sdl2glcontext) != 0) {
    fprintf(stderr, "SDL_GL_MakeCurrent failed during presentation: %s\n",
            SDL_GetError());
    return;
  }

  int width = 0, height = 0;
  SDL_GL_GetDrawableSize(sdl2window, &width, &height);
  if (width <= 0 || height <= 0) {
    if (!g_singlethreaded) {
      SDL_GL_MakeCurrent(sdl2window, NULL);
    }
    return;
  }

  glViewport(0, 0, width, height);
  glClearColor(0.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);

  if (SDL_MUSTLOCK(frame) && SDL_LockSurface(frame) != 0) {
    fprintf(stderr, "SDL_LockSurface failed for GLES upload: %s\n", SDL_GetError());
    if (!g_singlethreaded) {
      SDL_GL_MakeCurrent(sdl2window, NULL);
    }
    return;
  }

  gles_texture_index ^= 1;
  const GLuint texture = gles_textures[gles_texture_index];

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);

  if (gles_texture_width[gles_texture_index] != frame->w ||
      gles_texture_height[gles_texture_index] != frame->h) {

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame->w, frame->h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    gles_texture_width[gles_texture_index] = frame->w;
    gles_texture_height[gles_texture_index] = frame->h;
  }

  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame->w, frame->h,
                  GL_RGBA, GL_UNSIGNED_BYTE, frame->pixels);

  if (SDL_MUSTLOCK(frame)) {
    SDL_UnlockSurface(frame);
  }

  const GLfloat vertices[] = {
    -1.f, -1.f, 0.f, 1.f,
     1.f, -1.f, 1.f, 1.f,
    -1.f,  1.f, 0.f, 0.f,
     1.f,  1.f, 1.f, 0.f
  };

  glUseProgram(gles_program);
  glUniform1i(gles_sampler, 0);
  glVertexAttribPointer(gles_position, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), vertices);
  glVertexAttribPointer(gles_texcoord, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), vertices + 2);
  glEnableVertexAttribArray(gles_position);
  glEnableVertexAttribArray(gles_texcoord);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  glDisableVertexAttribArray(gles_position);
  glDisableVertexAttribArray(gles_texcoord);

  SDL_GL_SwapWindow(sdl2window);

  if (!g_singlethreaded) {
    SDL_GL_MakeCurrent(sdl2window, NULL); // Gemini
  }
}

// Do not redirect any GL names outside this SDL2 GLES implementation.
#undef glCreateShader
#undef glShaderSource
#undef glCompileShader
#undef glGetShaderiv
#undef glGetShaderInfoLog
#undef glGetError
#undef glDeleteShader
#undef glCreateProgram
#undef glAttachShader
#undef glBindAttribLocation
#undef glLinkProgram
#undef glDeleteProgram
#undef glGetProgramiv
#undef glGetProgramInfoLog
#undef glGetAttribLocation
#undef glGetUniformLocation
#undef glGenTextures
#undef glBindTexture
#undef glTexParameteri
#undef glPixelStorei
#undef glDisable
#undef glGetString
#undef glViewport
#undef glClearColor
#undef glClear
#undef glActiveTexture
#undef glTexImage2D
#undef glTexSubImage2D
#undef glUseProgram
#undef glUniform1i
#undef glVertexAttribPointer
#undef glEnableVertexAttribArray
#undef glDrawArrays
#undef glDisableVertexAttribArray
#undef glDeleteTextures
#endif

SDL_Surface *apple_icon;
SDL_Surface *screen;  // our main screen
// rects for screen stretch if needed
SDL_Rect origRect;
SDL_Rect newRect;

#define  VIEWPORTCX  560
#if ENABLE_MENU
#define  VIEWPORTCY  400
#else
#define  VIEWPORTCY  384
#endif
#define  BUTTONX     (VIEWPORTCX+(VIEWPORTX<<1))
#define  BUTTONY     0
#define  BUTTONCX    45
#define  BUTTONCY    45
#define  FSVIEWPORTX (640-BUTTONCX-VIEWPORTX-VIEWPORTCX)
#define  FSVIEWPORTY ((480-VIEWPORTCY)>>1)
#define  FSBUTTONX   (640-BUTTONCX)
#define  FSBUTTONY   (((480-VIEWPORTCY)>>1)-1)
#define  BUTTONS     8

static bool g_bAppActive = false;

static int buttondown = -1;

bool fullscreen = 0;
bool g_WindowResized;  // if we do not have a normal window size

static bool usingcursor = 0;

void DrawStatusArea(int drawflags);

void ProcessButtonClick(int button, int mod); // handle control buttons(F1-..F12) events

void ResetMachineState();

void SetFullScreenMode();

void SetNormalMode();

void SetUsingCursor(bool);

bool g_bScrollLock_FullSpeed = false;  // no in full speed!

void DrawFrameWindow()
{
  VideoRealizePalette(/*dc*/);

  // DRAW THE STATUS AREA
  DrawStatusArea(DRAW_BACKGROUND | DRAW_LEDS);

  // DRAW THE CONTENTS OF THE EMULATED SCREEN
  if (g_nAppMode == MODE_LOGO) {
    VideoDisplayLogo();
  } else if (g_nAppMode == MODE_DEBUG) {
    DebugDisplay(1);
  } else {
    VideoRedrawScreen(); // normal state - running emulator?
  }
}

void DrawStatusArea(int drawflags)
{
  if (font_sfc == NULL) {
    if (!fonts_initialization()) {
      fprintf(stderr, "Font file was not loaded.\n");
      return; // if we don't have a font, we just can do none
    }
  }

  SDL_Rect srect;
#ifdef SDL2
  Uint32 mybluez = SDL_MapRGB(g_hStatusSurface->format, 10, 10, 255);  // bluez color, know that?
//  SDL_SetPaletteColors(g_hStatusSurface->format->palette, screen->format->palette->colors, 0, 256);
#else
  Uint32 mybluez = SDL_MapRGB(screen->format, 10, 10, 255);  // bluez color, know that?
  SDL_SetColors(g_hStatusSurface, screen->format->palette->colors, 0, 256);
#endif

  if (drawflags & DRAW_BACKGROUND) {
    g_iStatusCycle = SHOW_CYCLES;  // start cycle for panel showing
  }
  if (drawflags & DRAW_LEDS) {
    srect.x = 4;
    srect.y = 22;
    srect.w = STATUS_PANEL_W - 8;
    srect.h = STATUS_PANEL_H - 25;
    SDL_FillRect(g_hStatusSurface, &srect, mybluez);  // clear

    char leds[2] = "\x64";
    #define LEDS  1
    int iDrive1Status = DISK_STATUS_OFF;
    int iDrive2Status = DISK_STATUS_OFF;
    int iHDDStatus = DISK_STATUS_OFF;

    DiskGetLightStatus(&iDrive1Status, &iDrive2Status);
    iHDDStatus = HD_GetStatus();

    leds[0] = LEDS + iDrive1Status;
    font_print(8, 23, leds, g_hStatusSurface, 4, 2.7);

    leds[0] = LEDS + iDrive2Status;
    font_print(40, 23, leds, g_hStatusSurface, 4, 2.7);

    leds[0] = LEDS + iHDDStatus;
    font_print(71, 23, leds, g_hStatusSurface, 4, 2.7);

    if (iDrive1Status | iDrive2Status | iHDDStatus) {
      g_iStatusCycle = SHOW_CYCLES;
    } // show status panel
  }
}

void FrameShowHelpScreen(int sx, int sy) // sx, sy - sizes of current window (screen)
{
  const int MAX_LINES = 25;
  const char *HelpStrings[MAX_LINES] = {"Welcome to LinApple - Apple][ emulator for Linux!",
                                        "Conf file is linapple.conf in current directory by default",
                                        "Hugest archive of Apple][ stuff you can find at ftp.apple.asimov.net",
                                        "       F1 - Show help screen",
                                        "  Ctrl+F2 - Cold reboot (Power off and back on)",
                                        " Shift+F2 - Reload configuration file and cold reboot",
                                        " Ctrl+F10 - Hot Reset (Control+Reset)",
                                        "      F12 - Quit",
                                        "",
                                        "    F3/F4 - Load floppy disk 1/2 (Slot 6, Drive 1/2)",
                                        "       F5 - Swap floppy disks",
                                        " Shift+F3/F4 - Attach hard drive 1/2 (Slot 7, Drive 1/2)",
                                        "",
                                        "       F6 - Toggle fullscreen mode",
                                        " Shift+F6 - Toggle character set (keyboard rocker switch)",
                                        "       F7 - Toggle debugging view",
                                        "       F8 - Take screenshot",
                                        " Shift+F8 - Save runtime changes to configuration file",
                                        "       F9 - Cycle through various video modes",
                                        " Shift+F9 - Budget video, for smoother music/audio",
                                        "  F10/F11 - Load/save snapshot file",
                                        "",
                                        "       Pause - Pause/resume emulator",
                                        " Scroll Lock - Toggle full speed",
                                        "  Numpad +/-/* - Increase/Decrease/Normal speed"};

  SDL_Surface *my_screen; // for background
  SDL_Surface *tempSurface = NULL; // temporary surface

  if (font_sfc == NULL) {
    if (!fonts_initialization()) {
      fprintf(stderr, "Font file was not loaded.\n");
      return; // If we don't have a font, we just can do none
    }
  }
  if (!g_WindowResized) {
    if (g_nAppMode == MODE_LOGO) {
      tempSurface = g_hLogoBitmap; // Use logobitmap
    } else {
      tempSurface = g_hDeviceBitmap;
    }
  } else {
    tempSurface = g_origscreen;
  }

  if (tempSurface == NULL) {
    tempSurface = screen;
  } // Use screen, if none available
#ifdef SDL2
  my_screen = SDL_ConvertSurface(tempSurface, screen->format, 0);
  //SDL_SetPaletteColors(my_screen->format->palette, tempSurface->format->palette->colors, 0, tempSurface->format->palette->ncolors);
  SDL_BlitScaled(tempSurface, NULL, my_screen, NULL);
  surfacefade32(my_screen, 0.2F);
  SDL_BlitScaled(my_screen, NULL, screen, NULL);    // show background
#else
  my_screen = SDL_CreateRGBSurface(SDL_SWSURFACE, tempSurface->w, tempSurface->h, tempSurface->format->BitsPerPixel, 0,
                                   0, 0, 0);
  if (tempSurface->format->palette && my_screen->format->palette) {
    SDL_SetColors(my_screen, tempSurface->format->palette->colors, 0, tempSurface->format->palette->ncolors);
  }
  surface_fader(my_screen, 0.2F, 0.2F, 0.2F, -1, 0);  // fade it out to 20% of normal
  SDL_BlitSurface(tempSurface, NULL, my_screen, NULL);
  SDL_BlitSurface(my_screen, NULL, screen, NULL);    // show background
#endif

  double facx = double(g_ScreenWidth) / double(SCREEN_WIDTH);
  double facy = double(g_ScreenHeight) / double(SCREEN_HEIGHT);

  font_print_centered(sx / 2, int(5 * facy), (char *) HelpStrings[0], screen, 1.5 * facx, 1.3 * facy);
  font_print_centered(sx / 2, int(20 * facy), (char *) HelpStrings[1], screen, 1.3 * facx, 1.2 * facy);
  font_print_centered(sx / 2, int(30 * facy), (char *) HelpStrings[2], screen, 1.2 * facx, 1.0 * facy);

  int Help_TopX = int(45 * facy);
  for (int i = 3; i < MAX_LINES; i++) {
    if (HelpStrings[i])
      font_print(4, Help_TopX + (i - 3) * 15 * facy, (char *) HelpStrings[i], screen, 1.5 * facx,
               1.5 * facy); // show keys
  }

  // show frames
  rectangle(screen, 0, Help_TopX - 5, g_ScreenWidth - 1, int(335 * facy), SDL_MapRGB(screen->format, 255, 255, 255));
  rectangle(screen, 1, Help_TopX - 4, g_ScreenWidth, int(335 * facy), SDL_MapRGB(screen->format, 255, 255, 255));
  rectangle(screen, 1, 1, g_ScreenWidth - 2, (Help_TopX - 8), SDL_MapRGB(screen->format, 255, 255, 0));

#ifdef SDL2
//  tempSurface = SDL_ConvertSurfaceFormat(assets->icon, SDL_GetWindowPixelFormat(sdl2window), 0);
  tempSurface = SDL_ConvertSurface(assets->icon, screen->format, 0);
#else
  tempSurface = SDL_DisplayFormat(assets->icon);
#endif

  SDL_Rect logo, scrr;
  logo.x = logo.y = 0;
  logo.w = tempSurface->w;
  logo.h = tempSurface->h;
  scrr.x = int(460 * facx);
  scrr.y = int(270 * facy);
  scrr.w = scrr.h = int(100 * facy);


#ifdef SDL2
  SDL_BlitScaled(tempSurface, &logo, screen, &scrr);
  SDL_FreeSurface(tempSurface);
  SDL_FreeSurface(my_screen);

  FramePresentGL(screen);
#else
  SDL_SoftStretchOr(tempSurface, &logo, screen, &scrr);
  SDL_Flip(screen); // Show the screen
#endif
  SDL_Delay(1000); // Wait 1 second to be not too fast

  // Wait for keypress
  SDL_Event event;

  event.type = SDL_QUIT;
  while (event.type != SDL_KEYDOWN) { // Wait for ESC-key pressed
    usleep(100);
    SDL_PollEvent(&event);
  }

  DrawFrameWindow(); // Restore screen
}

void FrameQuickState(int num, int mod)
{
  // quick load or save state with number num, if Shift is pressed, state is being saved, otherwise - being loaded
  char fpath[MAX_PATH];
  snprintf(fpath, MAX_PATH, "%.*s/SaveState%d.aws", int(strlen(g_sSaveStateDir)), g_sSaveStateDir, num); // prepare file name
  Snapshot_SetFilename(fpath);  // set it as a working name
  if (mod & KMOD_SHIFT) {
    Snapshot_SaveState();
  } else {
    Snapshot_LoadState();
  }
}

static bool bIamFullScreened;  // for correct fullscreen switching

#ifdef SDL2
static int sdl2windowflags;
#endif

void FrameDispatchMessage(SDL_Event *e) {// process given SDL event

  // OSK will be invoked using 'Esc' for now
  if (OSK_IsVisible() && e->type != SDL_QUIT)
  {
    OSK_HandleEvent(e);
    return;
  }

  if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_ESCAPE)
  {
    if (g_nAppMode == MODE_LOGO) {
      return;
    }
    OSK_Show();
    return;
  }

  int mysym = e->key.keysym.sym; // keycode
  int mymod = e->key.keysym.mod; // some special keys flags
  int myscancode = e->key.keysym.scancode; // some special keys flags
  int x, y; // used for mouse cursor position

  // Unicode Translated character
  if (g_KeyboardLanguage == Spanish_ES && mysym == 0)
  {
#ifdef SDL2
    // SDL2 lacks unicode support
#else
    mysym = e->key.keysym.unicode;
#endif
  }

  switch (e->type) {//type of SDL event

#ifdef SDL2

    case SDL_WINDOWEVENT:
      if (e->window.event == SDL_WINDOWEVENT_RESIZED) {

        g_WindowResized = false;

        if ((g_nAppMode != MODE_LOGO) &&
            (g_nAppMode != MODE_DEBUG)) {
          VideoRedrawScreen();
        }
      }
      else if (e->window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
        g_bAppActive = true;
      }
      else if (e->window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
        g_bAppActive = false;
      }
      break;

#else

    case SDL_VIDEORESIZE:
      printf("OLD DIMENSIONS: %d  %d\n", g_ScreenWidth, g_ScreenHeight);
      g_ScreenWidth = e->resize.w;
      g_ScreenHeight = (e->resize.h / 96) * 96;
      if (g_ScreenHeight < 192) {
        g_ScreenHeight = 192;
      }
      // Resize the screen
      screen = SDL_SetVideoMode(e->resize.w, e->resize.h, SCREEN_BPP,
                                SDL_SWSURFACE | SDL_HWPALETTE | SDL_RESIZABLE);

      if (screen == NULL) {
        SDL_Quit();
        return;
      } else {
        // Define if we have resized window
        g_WindowResized = (g_ScreenWidth != SCREEN_WIDTH) | (g_ScreenHeight != SCREEN_HEIGHT);
        printf("Screen size is %dx%d\n", g_ScreenWidth, g_ScreenHeight);
        if (g_WindowResized) {
          // create rects for screen stretching
          origRect.x = origRect.y = newRect.x = newRect.y = 0;
          origRect.w = SCREEN_WIDTH;
          origRect.h = SCREEN_HEIGHT;
          newRect.w = g_ScreenWidth;
          newRect.h = g_ScreenHeight;
          if ((g_nAppMode != MODE_LOGO) && (g_nAppMode != MODE_DEBUG)) {
            VideoRedrawScreen();
          }
        }
      }
      break;

    case SDL_ACTIVEEVENT:
      g_bAppActive = e->active.gain; // if gain==1, app is active
      break;

#endif

    case SDL_KEYDOWN:
      if (mysym == SDLK_ESCAPE) {
        // Toggle on-screen keyboard
        OSK_Toggle();
        break;
      }

      if (mysym >= SDLK_0 && mysym <= SDLK_9 && mymod & KMOD_LCTRL) {
        FrameQuickState(mysym - SDLK_0, mymod);
        break;
      }

      if ((mysym >= SDLK_F1) && (mysym <= SDLK_F12) && (buttondown == -1)) {
        SetUsingCursor(0);
        buttondown = mysym - SDLK_F1;  // special function keys processing
      } else if (mysym == SDLK_KP_PLUS) { // Gray + - speed up the emulator!
        g_dwSpeed = g_dwSpeed + 2;
        if (g_dwSpeed > SPEED_MAX) {
          g_dwSpeed = SPEED_MAX;
        } // no Maximum trespassing!
        printf("Now speed=%d\n", (int) g_dwSpeed);
        SetCurrentCLK6502();
      } else if (mysym == SDLK_KP_MINUS) { // Gray + - speed up the emulator!
        if (g_dwSpeed > SPEED_MIN) {
          g_dwSpeed = g_dwSpeed - 1;
        }// dw is unsigned value!
        printf("Now speed=%d\n", (int) g_dwSpeed);
        SetCurrentCLK6502();
      } else if (mysym == SDLK_KP_MULTIPLY) { // Gray * - normal speed!
        g_dwSpeed = 10;// dw is unsigned value!
        printf("Now speed=%d\n", (int) g_dwSpeed);
        SetCurrentCLK6502();
      } else if (mysym == SDLK_CAPSLOCK) {
        KeybToggleCapsLock();
      } else if (mysym == SDLK_PAUSE) {
        SetUsingCursor(0); // release cursor?
        switch (g_nAppMode) {
          case MODE_RUNNING: // go in pause
            g_nAppMode = MODE_PAUSED;
            SoundCore_SetFade(FADE_OUT); // fade out sound?**************
            break;
          case MODE_PAUSED: // go to the normal mode?
            g_nAppMode = MODE_RUNNING;
            SoundCore_SetFade(FADE_IN);  // fade in sound?***************
            break;
          case MODE_STEPPING:
            DebuggerInputConsoleChar(DEBUG_EXIT_KEY);
            break;
          case MODE_LOGO:
          case MODE_DEBUG:
          default:
            break;
        }
        DrawStatusArea(DRAW_TITLE);
        if ((g_nAppMode != MODE_LOGO) && (g_nAppMode != MODE_DEBUG)) {
          VideoRedrawScreen();
        }
        g_bResetTiming = true;
#ifdef SDL2
      } else if (mysym == SDLK_SCROLLLOCK) {
#else
      } else if (mysym == SDLK_SCROLLOCK) {
#endif
        g_bScrollLock_FullSpeed = !g_bScrollLock_FullSpeed; // turn on/off full speed?
      } else if ((g_nAppMode == MODE_RUNNING) || (g_nAppMode == MODE_LOGO) || (g_nAppMode == MODE_STEPPING)) {
        g_bDebuggerEatKey = false;
        // Note about Alt Gr (Right-Alt):
        // . WM_KEYDOWN[Left-Control], then:
        // . WM_KEYDOWN[Right-Alt]
        bool autorep = 0; //previous key was pressed? 30bit of lparam
#ifdef SDL2
        bool extended =
            (mysym == SDLK_UP) ||
            (mysym == SDLK_DOWN) ||
            (mysym == SDLK_LEFT) ||
            (mysym == SDLK_RIGHT);
#else
        bool extended = (mysym >= SDLK_UP); // 24bit of lparam - is an extended key, what is it???
#endif
        if (mymod & KMOD_RCTRL)     // GPH: Update trim?
        {
          JoyUpdateTrimViaKey(mysym);
        } else {
          // Regular joystick movement
          if ((!JoyProcessKey(mysym, extended, true, autorep)) && (g_nAppMode != MODE_LOGO)) {
            KeybQueueKeypress(mysym, NOT_ASCII);
          }
        }
      } else if (g_nAppMode == MODE_DEBUG) {
        if (((mymod & (KMOD_LSHIFT|KMOD_RSHIFT|KMOD_CAPS))>0)&&
            ((mysym>='a')&&(mysym<='z')))
        {
          // convert to upper case when any shift key was pressed
          mysym += 'A'-'a';
        }
        else
        {
          KeybUpdateCtrlShiftStatus();
          mysym = KeybDecodeKey(mysym);
        }
        DebuggerProcessKey(mysym);
      }
      break;

    case SDL_KEYUP:
      if ((mysym >= SDLK_F1) && (mysym <= SDLK_F12) && (buttondown == mysym - SDLK_F1)) {
        buttondown = -1;
        ProcessButtonClick(mysym - SDLK_F1, mymod); // process function keys - special events
      } else if (mysym == SDLK_CAPSLOCK) {
        // GPH Fix caps lock toggle behavior.
        // (http://sdl.beuc.net/sdl.wiki/SDL_KeyboardEvent)
        KeybToggleCapsLock();
      } else {  // Need to know what "extended" means, and what's so special about SDLK_UP?
        if (myscancode) { // GPH: Checking scan codes tells us if a key was REALLY released.
#ifdef SDL2
          JoyProcessKey(mysym,
                        (mysym == SDLK_UP) ||
                        (mysym == SDLK_DOWN) ||
                        (mysym == SDLK_LEFT) ||
                        (mysym == SDLK_RIGHT),
                        false, 0);
#else
          JoyProcessKey(mysym, (mysym >= SDLK_UP && mysym <= SDLK_LEFT), false, 0);
#endif
        }
      }
      break;

    case SDL_MOUSEBUTTONDOWN:
      if (e->button.button == SDL_BUTTON_LEFT) {
        if (buttondown == -1) {
          x = e->button.x;
          y = e->button.y;
          if (g_nAppMode == MODE_DEBUG)
            DebuggerMouseClick(x, y);
          else
          if (usingcursor) {
            KeybUpdateCtrlShiftStatus(); // if either of ALT, SHIFT or CTRL is pressed
            if (g_bShiftKey | g_bCtrlKey) {
              SetUsingCursor(0); // release mouse cursor for user
            } else {
              if (sg_Mouse.Active()) {
                sg_Mouse.SetButton(BUTTON0, BUTTON_DOWN);
              } else {
                JoySetButton(BUTTON0, BUTTON_DOWN);
              }
            }
          } // we do not use mouse
          else
          if ((((g_nAppMode == MODE_RUNNING) || (g_nAppMode == MODE_STEPPING))) ||
              (sg_Mouse.Active())) {
            SetUsingCursor(1); // capture cursor
          }
        }
      } // If left mouse button down
      else if (e->button.button == SDL_BUTTON_RIGHT) {
        if (usingcursor) {
          if (sg_Mouse.Active()) {
            sg_Mouse.SetButton(BUTTON1, BUTTON_DOWN);
          } else {
            JoySetButton(BUTTON1, BUTTON_DOWN);
          }
        }
      }

      break;

    case SDL_MOUSEBUTTONUP:
      if (e->button.button == SDL_BUTTON_LEFT) {
        if (usingcursor) {
          if (sg_Mouse.Active()) {
            sg_Mouse.SetButton(BUTTON0, BUTTON_UP);
          } else {
            JoySetButton(BUTTON0, BUTTON_UP);
          }
        }
      } else if (e->button.button == SDL_BUTTON_RIGHT) {
        if (usingcursor) {
          if (sg_Mouse.Active()) {
            sg_Mouse.SetButton(BUTTON1, BUTTON_UP);
          } else {
            JoySetButton(BUTTON1, BUTTON_UP);
          }
        }
      }
      break;

    case SDL_MOUSEMOTION:
      x = e->motion.x; // Get relative coordinates of mouse cursor
      y = e->motion.y;
      if (usingcursor) {
        if (sg_Mouse.Active()) {
          sg_Mouse.SetPosition(x, VIEWPORTCX - 4, y, VIEWPORTCY - 4);
        } else {
          JoySetPosition(x, VIEWPORTCX - 4, y, VIEWPORTCY - 4);
        }
      }
      break;

    case SDL_USEREVENT:
      if (e->user.code == 1) { // should do restart?
        ProcessButtonClick(BTN_RUN, KMOD_LCTRL);
      }
      break;

  }
}

bool PSP_SaveStateSelectImage(bool saveit)
{
  static size_t fileIndex = 0;    // file index will be remembered for current dir
  static int backdx = 0;  // reserve
  static int dirdx = 0;  // reserve for dirs

  std::string filename;      // given filename
  std::string fullPath;  // full path for it
  bool isDirectory = true;      // if given filename is a directory?

  fileIndex = backdx;
  fullPath = g_sSaveStateDir;  // global var for disk selecting directory

  while (isDirectory) {
    if (!ChooseAnImage(g_ScreenWidth, g_ScreenHeight, fullPath, saveit,
                       filename, isDirectory, fileIndex)) {
      DrawFrameWindow();
      return false;  // if ESC was pressed, just leave
    }
    if (isDirectory) {
      if (filename == "..") {
        const auto last_sep_pos = fullPath.find_last_of(FILE_SEPARATOR);
        if (last_sep_pos == std::string::npos) {
          fullPath = fullPath.substr(0, last_sep_pos);
        }
        if (fullPath == "") {
          fullPath = "/";  //we don't want fullPath to be empty
        }
        fileIndex = dirdx;  // restore
      } else {
        if (fullPath != "/") {
          fullPath += "/" + filename;
        } else {
          fullPath = "/" + filename;
        }
        dirdx = fileIndex; // store it
        fileIndex = 0;  // start with beginning of dir
      }
    }
  }
  strcpy(g_sSaveStateDir, fullPath.c_str());
  RegSaveString(TEXT("Preferences"), REGVALUE_PREF_SAVESTATE_DIR, 1, g_sSaveStateDir); // Save it

  backdx = fileIndex; // Store cursor position

  fullPath += "/" + filename;

  Snapshot_SetFilename(fullPath.c_str()); // Set name for snapshot
  RegSaveString(TEXT("Preferences"), REGVALUE_SAVESTATE_FILENAME, 1, fullPath.c_str()); // Save it
  DrawFrameWindow();
  return true;
}

void FrameSaveBMP(void) {
  // Save current screen as a .bmp file in current directory
  struct stat bufp;
  static int i = 1;  // index
  char bmpName[20];  // file name

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
  snprintf(bmpName, 20, "linapple%7d.bmp", i);
  while (!stat(bmpName, &bufp)) { // Find first absent file
    i++;
    snprintf(bmpName, 20, "linapple%7d.bmp", i);
  }
#pragma GCC diagnostic pop

#ifdef SDL2
  if (!g_WindowResized) {
    if (g_nAppMode == MODE_LOGO) {
      SDL_SaveBMP(g_hLogoBitmap, bmpName);  // Save file using SDL inner function
    } else {
      SDL_SaveBMP(g_hDeviceBitmap, bmpName);  // Save file using SDL inner function
    }
  } else {
    SDL_SaveBMP(g_origscreen, bmpName);  // Save file using SDL inner function
  }
#else
  SDL_SaveBMP(screen, bmpName);  // Save file using SDL inner function
#endif

  printf("File %s saved!\n", bmpName);
  i++;
}

void ProcessButtonClick(int button, int mod)
{
  // button - number of button pressed (starting with 0, which means F1
  // mod - what modifiers been set (like CTRL, ALT etc.)
  SDL_Event qe;  // for Quitting and Reset

  SoundCore_SetFade(FADE_OUT); // sound/music off?

  switch (button) {
    case BTN_HELP:
      FrameShowHelpScreen(screen->w, screen->h);
      break;

    case BTN_RUN:  // F2 - Run that thing! Or Shift+2 ReloadConfig and run it anyway!
      if ((mod & (KMOD_LCTRL)) == (KMOD_LCTRL) || (mod & (KMOD_RCTRL)) == (KMOD_RCTRL)) {
        if (g_nAppMode == MODE_LOGO) {
          DiskBoot();
        } else if (g_nAppMode == MODE_RUNNING) {
          ResetMachineState();
        }
        if ((g_nAppMode == MODE_DEBUG) || (g_nAppMode == MODE_STEPPING)) {
          DebugEnd();
        }
        g_nAppMode = MODE_RUNNING;
        DrawStatusArea(DRAW_TITLE);
        VideoRedrawScreen();
        g_bResetTiming = true;
      } else if (mod & KMOD_SHIFT) {
        restart = 1;  // Keep up flag of restarting
        qe.type = SDL_QUIT;
        SDL_PushEvent(&qe); // Push quit event
      }
      break;

    case BTN_DRIVE1:
    case BTN_DRIVE2:
      JoyReset();
      if (mod & KMOD_CTRL) {
        if (mod & KMOD_SHIFT) {
          printf("HDD  Eject Drive #%d\n", (button - BTN_DRIVE1) + 1);
          HD_Eject(button - BTN_DRIVE1);
        } else {
          printf("Disk Eject Drive #%d\n", (button - BTN_DRIVE1) + 1);
          DiskEject(button - BTN_DRIVE1);
        }
        break;
      }

      if (mod & KMOD_SHIFT) {
        if (mod & KMOD_ALT) {
          HD_FTP_Select(button - BTN_DRIVE1);// select HDV image through FTP
        } else {
          HD_Select(button - BTN_DRIVE1);
        }  // select HDV image from local disk
      } else {
        if (mod & KMOD_ALT) {
          Disk_FTP_SelectImage(button - BTN_DRIVE1);//select through FTP
        } else {
          DiskSelect(button - BTN_DRIVE1);
        } // select image file for appropriate disk drive(#1 or #2)
      }
      break;

    case BTN_DRIVESWAP:  // F5 - swap disk drives
      DiskDriveSwap();
      break;

    case BTN_FULLSCR:  // F6 - Fullscreen on/off
      if (mod & KMOD_SHIFT) {
         // only IIe and enhanced have a keyboard rocker switch (and only non-US keyboards)
         if ((g_KeyboardLanguage != English_US)&&
             ((g_Apple2Type == A2TYPE_APPLE2E)||(g_Apple2Type == A2TYPE_APPLE2EEHANCED)))
         {
           g_KeyboardRockerSwitch = !g_KeyboardRockerSwitch;
           printf("Toggling keyboard rocker switch. Selected character set: %s...\n", (g_KeyboardRockerSwitch) ? "local" : "standard/US");
         }
      }
      else
      {
        if (fullscreen) {
          fullscreen = 0;
          SetNormalMode();
        } else {
          fullscreen = 1;
          SetFullScreenMode();
        }
        JoyReset();   // GPH: Sometimes lose ability to use buttons after switch
      }
      break;

    case BTN_DEBUG:  // F7 - debug mode - not implemented yet? Please, see README about it. --bb
      if (g_nAppMode != MODE_DEBUG)
      {
        DebugBegin();
        SetUsingCursor(0);
      }
      else
      if (g_nAppMode == MODE_DEBUG)
      {
        g_nAppMode = MODE_RUNNING;
      }
      break;

    case BTN_SETUP:  // setup is in conf file - linapple.conf.
      // may be it should be implemented using SDL??? 0_0 --bb
      // Now Shift-F8 save settings changed run-tme in linapple.conf
      // F8 - save current screen as a .bmp file
      // Currently these setting are just next:
      if (mod & KMOD_SHIFT) {
        RegSaveValue(TEXT("Configuration"), TEXT("Video Emulation"), 1, g_videotype);
        RegSaveValue(TEXT("Configuration"), TEXT("Emulation Speed"), 1, g_dwSpeed);
        RegSaveValue(TEXT("Configuration"), TEXT("Fullscreen"), 1, fullscreen);
      } else {
        FrameSaveBMP();
      }
      break;


    // Buttons handlers for F9 - F12
    case BTN_CYCLE: // F9 - CYCLE through allowed video modes
      if (mod & KMOD_SHIFT) {
        // GPH Added budget video for updating only every 12 60Hz frames.
        // This is because, on computers without a fast GPU, the drawing of the screen
        // can actually affect the audio.
        SetBudgetVideo(!GetBudgetVideo());
      } else {
        g_videotype++;  // Cycle through available video modes
        if (g_videotype >= VT_NUM_MODES) {
          g_videotype = 0;
        }
        VideoReinitialize();
        if (g_nAppMode != MODE_LOGO)
        {
          if (g_nAppMode == MODE_DEBUG)
          {
            unsigned int debugVideoMode;
            if (DebugGetVideoMode(&debugVideoMode)) {
              VideoRefreshScreen();
            }
          }
          else
          {
            VideoRefreshScreen();
          }
        }
      }
      break;
    case BTN_QUIT:  // F10 - exit from emulator?
      qe.type = SDL_QUIT;
      SDL_PushEvent(&qe); // Push quit event
      break;  //
    case BTN_SAVEST: // Save state (F11)
      if (mod & KMOD_ALT) { // quick save
        Snapshot_SaveState();
      } else if (PSP_SaveStateSelectImage(true)) {
        Snapshot_SaveState();
      }
      break;
    case BTN_LOADST: // Load state (F12) or Hot Reset (Ctrl+F12)
      if (mod & KMOD_CTRL) {
        if (!IS_APPLE2) {
          MemResetPaging();
        }

        DiskReset();
        KeybReset();
        if (!IS_APPLE2) {
          VideoResetState();
        }  // Switch Alternate char set off
        MB_Reset();
        CpuReset();
      } else if (mod & KMOD_ALT) { // quick load state
        Snapshot_LoadState();
      } else if (PSP_SaveStateSelectImage(false)) {
        Snapshot_LoadState();
      }
      break;
  }

  if ((g_nAppMode != MODE_DEBUG) && (g_nAppMode != MODE_PAUSED)) {
    SoundCore_SetFade(FADE_IN);
  }
}

void ResetMachineState() {
  DiskReset();    // Set floppymotoron=0
  g_bFullSpeed = 0;  // Might've hit reset in middle of InternalCpuExecute() - so beep may get (partially) muted

  MemReset();
  DiskBoot();
  VideoResetState();
  sg_SSC.CommReset();
  PrintReset();
  JoyReset();
  MB_Reset();
  SpkrReset();
}

void SetFullScreenMode() {
  if (!bIamFullScreened) {
    bIamFullScreened = true;
#ifdef SDL2
    sdl2windowflags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    SDL_SetWindowFullscreen(sdl2window, sdl2windowflags);
#else
    SDL_WM_ToggleFullScreen(screen);
#endif
    if (g_nAppMode != MODE_DEBUG)
      SDL_ShowCursor(SDL_DISABLE);
  }
}

void SetNormalMode()
{
  if (bIamFullScreened) {
    bIamFullScreened = 0;
#ifdef SDL2
    sdl2windowflags = 0;
    SDL_SetWindowFullscreen(sdl2window, sdl2windowflags);
#else
    SDL_WM_ToggleFullScreen(screen);// we should go back anyway!? ^_^  --bb
#endif
    if (!usingcursor) {
      SDL_ShowCursor(SDL_ENABLE);
    } // show mouse cursor if not use it
  }
  else
  if (g_nAppMode == MODE_DEBUG)
  {
#ifdef SDL2
    SDL_SetRelativeMouseMode(SDL_FALSE);
#else
    SDL_ShowCursor(SDL_ENABLE);
    SDL_WM_GrabInput(SDL_GRAB_OFF);
#endif
  }
}

void SetUsingCursor(bool newvalue) {
  usingcursor = newvalue;
  if (usingcursor) { // Hide mouse cursor and grab input (mouse and keyboard)
#ifdef SDL2
    SDL_SetRelativeMouseMode(SDL_TRUE);
#else
    SDL_ShowCursor(SDL_DISABLE);
    SDL_WM_GrabInput(SDL_GRAB_ON);
#endif
  } else { // On the contrary - show mouse cursor and ungrab input

#ifdef SDL2
    SDL_SetRelativeMouseMode(SDL_FALSE);
#else
    if ((!bIamFullScreened)||(g_nAppMode == MODE_DEBUG)) {
      SDL_ShowCursor(SDL_ENABLE);
    }  // Show cursor if not in fullscreen mode
    SDL_WM_GrabInput(SDL_GRAB_OFF);
#endif
  }
}

int FrameCreateWindow()
{
  // Init SDL and create window screen

#ifdef SDL2
  SDL_setenv("SDL_VIDEO_CENTERED", "center", 1); // Center our window
#else
  static char sdlCmd[] = "SDL_VIDEO_CENTERED=center";
  SDL_putenv(sdlCmd); // Center our window
#endif

  bIamFullScreened = false; // At startup not in fullscreen mode
#ifdef SDL2

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);

  sdl2window = SDL_CreateWindow(g_pAppTitle,
                                SDL_WINDOWPOS_UNDEFINED,
                                SDL_WINDOWPOS_UNDEFINED,
                                g_ScreenWidth, g_ScreenHeight,
                                sdl2windowflags | SDL_WINDOW_OPENGL);

  if (!sdl2window) {
    fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    return 1;
  }

  sdl2glcontext = SDL_GL_CreateContext(sdl2window);
  if (!sdl2glcontext) {
    fprintf(stderr, "SDL_GL_CreateContext (OpenGL ES 2.0) failed: %s\n", SDL_GetError());
    return 1;
  }

  if (SDL_GL_MakeCurrent(sdl2window, sdl2glcontext) != 0) {
    fprintf(stderr, "SDL_GL_MakeCurrent failed: %s\n", SDL_GetError());
    SDL_GL_DeleteContext(sdl2glcontext);
    sdl2glcontext = NULL;
    return 1;
  }

  SDL_GL_SetSwapInterval(0);

  if (!GLESInitialize()) {
    fprintf(stderr, "Could not initialize OpenGL ES 2.0 renderer.\n");
    FrameShutdownGL();
    return 1;
  }

  screen = SDL_CreateRGBSurfaceWithFormat(0, g_ScreenWidth, g_ScreenHeight, 32, SDL_PIXELFORMAT_RGBA32);

  SDL_GL_MakeCurrent(sdl2window, NULL);

#else
  screen = SDL_SetVideoMode(g_ScreenWidth, g_ScreenHeight, SCREEN_BPP, SDL_SWSURFACE | SDL_HWPALETTE);
#endif
  if (screen == NULL) {
    fprintf(stderr, "Could not set SDL video mode: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  // define if we have resized window
  g_WindowResized = (g_ScreenWidth != SCREEN_WIDTH) | (g_ScreenHeight != SCREEN_HEIGHT);
  printf("Screen size is %dx%d\n", g_ScreenWidth, g_ScreenHeight);
  if (g_WindowResized) {
    // create rects for screen stretching
    origRect.x = origRect.y = newRect.x = newRect.y = 0;
    origRect.w = SCREEN_WIDTH;
    origRect.h = SCREEN_HEIGHT;
    newRect.w = g_ScreenWidth;
    newRect.h = g_ScreenHeight;
  }
#ifdef SDL2
  // TODO: Key repeat
#else
  SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
#endif
  return 0;
}

void SetIcon()
{
  /* Black is the transparency colour.
     Part of the logo seems to use it !? */
  Uint32 colorkey = SDL_MapRGB(assets->icon->format, 0, 0, 0);
#ifdef SDL2
  SDL_SetColorKey(assets->icon, SDL_TRUE, colorkey);
#else
  SDL_SetColorKey(assets->icon, SDL_SRCCOLORKEY, colorkey);

  /* No need to pass a mask given the above. */
  SDL_WM_SetIcon(assets->icon, NULL);
#endif
}

int InitSDL()
{
  // initialize SDL subsystems, return 0 if all OK, else return 1
  if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
    fprintf(stderr, "Could not initialize SDL: %s\n", SDL_GetError());
    return 1;
  }

  // SDL ref: Icon should be set *before* the first call to SDL_SetVideoMode.
  SetIcon();
  return 0;
}

void FrameRefreshStatus(int drawflags)
{
  DrawStatusArea(drawflags);
}
