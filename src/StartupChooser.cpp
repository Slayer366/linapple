#include "stdafx.h"
#include "StartupChooser.h"
#include "asset.h"

#include <string>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

extern int DoDiskInsert(int nDrive, char *szFileName);
extern bool hddenabled;

namespace {

static const size_t kMaxDisplayedFilename = 55; // Leave room for the entry type/size at the right edge.
static const char kEjectEntryName[] = "[EJECT / UNMOUNT CURRENT IMAGE]";

std::string baseName(const char *path);

static int PortMasterDetected = false;

bool hasMountedMedia(const char *name)
{
  return name != NULL && name[0] != '\0';
}

// Match Snapshot_ResolveFilename(): floppy drive 1 takes priority unless it
// contains the automatically mounted Master.dsk, in which case use HDD 1.
std::string gptokeybImageName()
{
  const char *floppy = DiskGetFullName(0);
  if (hasMountedMedia(floppy)) {
    const std::string floppyName = baseName(floppy);
    if (strcasecmp(floppyName.c_str(), "Master.dsk") != 0) {
      return floppyName;
    }
  }

  const char *hardDisk = HD_GetFullName(0);
  if (hasMountedMedia(hardDisk)) {
    return baseName(hardDisk);
  }

  return std::string();
}

std::string joinPath(const std::string& dir, const std::string& name)
{
  if (dir == "/") {
    return dir + name;
  }

  return dir + "/" + name;
}

void setupAndLaunchGptokeyb()
{
  const char *deviceArch = getenv("DEVICE_ARCH");
  if (!deviceArch) {
    return;
  }

  const char *esudo = getenv("ESUDO");
  if (!esudo) {
    esudo = "";
  }

  const char *gptoKeyb = getenv("GPTOKEYB");

  const char *gptoKeyb2 = getenv("GPTOKEYB2");
  if (!gptoKeyb2) {
    printf("GPTOKEYB2 not detected.\n");
  }

  const char *gameDir = getenv("GAMEDIR");
  if (!gameDir) {
    printf("GAMEDIR not set. Defaulting to: .\n");
    gameDir = ".";
  } else {
    printf("GAMEDIR=%s\n", gameDir);
  }

  std::string imageName = gptokeybImageName();
  if (imageName.empty()) {
    return;
  }

  std::string::size_type dot = imageName.find_last_of('.');
  if (dot != std::string::npos) {
    imageName.erase(dot);
  }

  if (imageName.empty()) {
    return;
  }

  char gptkPath[512];
  snprintf(gptkPath, sizeof(gptkPath), "%s/controls/%s.gptk", gameDir, imageName.c_str());

    // Check if file exists
    if (access(gptkPath, F_OK) == -1) {
        // File does not exist, copy default
        char defaultGptk[512];
        snprintf(defaultGptk, sizeof(defaultGptk), "%s/controls/linapple.gptk", gameDir);
        printf("Creating missing gptk config: %s\n", gptkPath);
        FILE *src = fopen(defaultGptk, "rb");
        FILE *dst = fopen(gptkPath, "wb");

        if (src && dst) {
            char buf[1024];
            size_t n;

            while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
                fwrite(buf, 1, n, dst);
            }
        }

        if (src) {
          fclose(src);
        }

        if (dst) {
          fclose(dst);
        }
    }

  // Kill any running gptokeyb
  if (system("killall -9 gptokeyb gptokeyb.aarch64 gptokeyb.armhf gptokeyb.x86_64 gptokeyb2 gptokeyb2.aarch64 gptokeyb2.armhf gptokeyb2.x86_64 >/dev/null 2>&1") == -1) {
    // don't return;
  }
  char killCommand[1024];
  snprintf(killCommand, sizeof(killCommand),
      "%s killall -9 gptokeyb gptokeyb.aarch64 gptokeyb.armhf "
      "gptokeyb.x86_64 gptokeyb2 gptokeyb2.aarch64 "
      "gptokeyb2.armhf gptokeyb2.x86_64 >/dev/null 2>&1",
      esudo);
  if (system(killCommand) == -1) {
    // don't return;
  }

  // Launch gptokeyb
  char gptkCommand[1024];
  snprintf(gptkCommand, sizeof(gptkCommand),
      "%s linapple.%s -c \"%s\" &",
      gptoKeyb,
      deviceArch,
      gptkPath
  );
  if (system(gptkCommand) == -1) {
    return;
  }
}

std::string parentPath(const std::string& path)
{
  if (path.empty()) {
    return ".";
  }

  std::string::size_type end = path.find_last_not_of('/');
  if (end == std::string::npos) {
    return "/";
  }

  std::string normalized = path.substr(0, end + 1);
  std::string::size_type slash = normalized.find_last_of('/');

  if (slash == std::string::npos) {
    return ".";
  }

  if (slash == 0) {
    return "/";
  }

  return normalized.substr(0, slash);
}

std::string baseName(const char *path)
{
  if (!path || !*path) {
    return "Empty";
  }

  std::string value(path);
  std::string::size_type slash = value.find_last_of('/');
  return slash == std::string::npos ? value : value.substr(slash + 1);
}

bool supportedExtension(const std::string& name, bool hardDisk)
{
  const std::string::size_type dot = name.find_last_of('.');
  if (dot == std::string::npos) {
    return false;
  }

  std::string ext = name.substr(dot);
  for (size_t i = 0; i < ext.size(); ++i) {
    ext[i] = static_cast<char>(tolower(static_cast<unsigned char>(ext[i])));
  }

  if (hardDisk) {
    const char *extensions[] = { ".hdv", ".2mg", ".2img", ".po" };
    for (size_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); ++i) {
      if (ext == extensions[i]) {
        return true;
      }
    }
  } else {
    const char *extensions[] = { ".dsk", ".do", ".po", ".2mg", ".2img", ".bin", ".nib", ".woz", ".zip", ".gz" };
    for (size_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); ++i) {
      if (ext == extensions[i]) {
        return true;
      }
    }
  }
  return false;
}

class ChooserFileList : public file_list_generator_t {
public:
  ChooserFileList(const std::string& directory, bool hardDisk)
    : m_directory(directory), m_hardDisk(hardDisk) {}

  const std::vector<file_entry_t> generate_file_list()
  {
    std::vector<file_entry_t> entries;
    if (m_directory != "/") {
      entries.push_back(file_entry_t("..", file_entry_t::UP, 0));
    }

    DIR *dir = opendir(m_directory.c_str());
    if (!dir) {
      m_failure = "Failed to list directory: " + m_directory;
      return std::vector<file_entry_t>();
    }

    struct dirent *item;
    while ((item = readdir(dir)) != NULL) {
      const char *name = item->d_name;
      if (!name[0] || name[0] == '.') {
        continue;
      }

      const std::string fullPath = joinPath(m_directory, name);
      struct stat info;
      if (stat(fullPath.c_str(), &info) != 0) {
        continue;
      }
      if (S_ISDIR(info.st_mode)) {
        entries.push_back(file_entry_t(name, file_entry_t::DIR, 0));
      } else if (S_ISREG(info.st_mode) && supportedExtension(name, m_hardDisk)) {
        entries.push_back(file_entry_t(name, file_entry_t::FILE, static_cast<std::uintmax_t>(info.st_size)));
      }
    }
    closedir(dir);
    std::sort(entries.begin(), entries.end());

    // ChooseImageDialog expects at least one row. Keep an inert-looking current
    // directory entry for the unusual case of an empty filesystem root.
    if (entries.empty()) {
      entries.push_back(file_entry_t(".", file_entry_t::DIR, 0));
    }
    m_failure = "(success)";
    return entries;
  }

  const std::string get_starting_message() { return "Reading media directory..."; }
  const std::string get_failure_message() { return m_failure; }

private:
  std::string m_directory;
  bool m_hardDisk;
  std::string m_failure = "(success)";
};

class ChooserBackground {
public:
  ChooserBackground() : m_surface(NULL)
  {
    // Match the background selection and fade used by DiskChoose.cpp / the help screen.
    pthread_mutex_lock(&video_draw_mutex);
    SDL_Surface *source = NULL;
    if (!g_WindowResized) {
      source = (g_nAppMode == MODE_LOGO) ? g_hLogoBitmap : g_hDeviceBitmap;
    } else {
      source = g_origscreen;
    }
    if (!source) {
      source = screen;
    }

#ifdef SDL2
    m_surface = SDL_ConvertSurface(source, screen->format, 0);
    if (m_surface) {
      // Copy the selected logo/device surface once, then fade that copy once.
      // draw() blits this immutable faded surface every frame, preventing trails.
      if (SDL_BlitScaled(source, NULL, m_surface, NULL) == 0) {
        surfacefade32(m_surface, 0.2F);
      }
    }
#else
    m_surface = SDL_CreateRGBSurface(SDL_SWSURFACE, source->w, source->h,
                                     source->format->BitsPerPixel, 0, 0, 0, 0);
    if (m_surface) {
      if (source->format->palette && m_surface->format->palette) {
        SDL_SetColors(m_surface, source->format->palette->colors, 0,
                      source->format->palette->ncolors);
      }
      // The source must be copied before applying the fade.
      if (SDL_BlitSurface(source, NULL, m_surface, NULL) == 0) {
        surface_fader(m_surface, 0.2F, 0.2F, 0.2F, -1, 0);
      }
    }
#endif
    pthread_mutex_unlock(&video_draw_mutex);
  }

  ~ChooserBackground() {
    if (m_surface) {
      SDL_FreeSurface(m_surface);
    }
  }

  void draw() const
  {
    if (!m_surface) {
      SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));
      return;
    }
#ifdef SDL2
    SDL_BlitScaled(m_surface, NULL, screen, NULL);
#else
    SDL_BlitSurface(m_surface, NULL, screen, NULL);
#endif
  }

private:
  SDL_Surface *m_surface;
};

void drawFrame(int sx, int headerBottom)
{
  const double fy = double(g_ScreenHeight) / double(SCREEN_HEIGHT);
  // Match the help screen's two-rectangle content border and yellow header.
  // Draw frames before text/highlights so borders cannot overwrite glyphs.
  // Start the white frame immediately below the yellow header, closing the gap.
  const int outerY = headerBottom - int(8 * fy) + 1;
  const int innerY = outerY + 1;
  const int outerHeight = screen->h - outerY - 2;
  const int innerHeight = screen->h - innerY - 2;
  rectangle(screen, 0, outerY, sx - 1, outerHeight,
            SDL_MapRGB(screen->format, 255, 255, 255));
  rectangle(screen, 1, innerY, sx - 2, innerHeight,
            SDL_MapRGB(screen->format, 255, 255, 255));
  rectangle(screen, 1, 1, sx - 2, headerBottom - int(8 * fy),
            SDL_MapRGB(screen->format, 255, 255, 0));
}

void drawMenu(const ChooserBackground& background, int selected)
{
  // Check unique envars to detect if PortMaster is present
  const char *deviceArch = getenv("DEVICE_ARCH");
  const char *gptoKeyb = getenv("GPTOKEYB");
  if (deviceArch && gptoKeyb) {
    PortMasterDetected = true;
  }

  const double fx = double(g_ScreenWidth) / double(SCREEN_WIDTH);
  const double fy = double(g_ScreenHeight) / double(SCREEN_HEIGHT);
  if (font_sfc == NULL && !fonts_initialization()) {
    return;
  }

  background.draw();
  drawFrame(screen->w, int(55 * fy));

  // Two-line header: keep the important warning, omit redundant navigation text.
  font_print_centered(screen->w / 2, int(7 * fy), "LinApple - Select Media", screen, 2.1 * fx, 2.0 * fy);
  font_print_centered(screen->w / 2, int(28 * fy), "ZIPs are not recommended for hard disks", screen, 2.1 * fx, 2.0 * fy);

  const char *labels[5] = { "5.25\" Drive 1:", "5.25\" Drive 2:", "Hard Disk Drive 1:", "Hard Disk Drive 2:", "  START" };
  const std::string names[4] = {
    baseName(DiskGetFullName(0)), baseName(DiskGetFullName(1)),
    baseName(HD_GetFullName(0)), baseName(HD_GetFullName(1))
  };
  const int top = int(61 * fy);
  const int blockStep = int(50 * fy);
  for (int i = 0; i < 5; ++i) {
    const int y = top + i * blockStep + (i == 4 ? int(12 * fy) : 0);
    if (i == selected) {
      SDL_Rect r;
      r.x = int(8 * fx);
      r.y = y - int(2 * fy);
      r.w = screen->w - int(16 * fx);
      r.h = i < 4 ? int(36 * fy) : int(18 * fy);
      SDL_FillRect(screen, &r, SDL_MapRGB(screen->format, 64, 128, 190));
    }
    font_print(int(16 * fx), y, labels[i], screen, 2.0 * fx, 2.0 * fy);
    if (i < 4) {
      const std::string shown = names[i].empty() ? "Empty" : names[i].substr(0, kMaxDisplayedFilename);
      font_print_nowrap(int(34 * fx), y + int(18 * fy), shown.c_str(), screen, 2.0 * fx, 2.0 * fy);
    }
  }

#ifdef SDL2
  SDL_Surface *appleIcon = SDL_ConvertSurface(assets->icon, screen->format, 0);
#else
  SDL_Surface *appleIcon = SDL_DisplayFormat(assets->icon);
#endif

  if (appleIcon) {
    SDL_Rect logo, scrr;
    logo.x = logo.y = 0;
    logo.w = appleIcon->w;
    logo.h = appleIcon->h;

    scrr.x = int(460 * fx);
    scrr.y = int(270 * fy);
    scrr.w = scrr.h = int(100 * fy);

#ifdef SDL2
    SDL_BlitScaled(appleIcon, &logo, screen, &scrr);
#else
    SDL_SoftStretchOr(appleIcon, &logo, screen, &scrr);
#endif
    SDL_FreeSurface(appleIcon);
  }

  if (PortMasterDetected) {
#ifdef SDL2
    SDL_Surface *pmIcon = SDL_ConvertSurface(assets->pm, screen->format, 0);
#else
    SDL_Surface *pmIcon = SDL_DisplayFormat(assets->pm);
#endif

    if (pmIcon) {
      SDL_Rect pm, pmscr;
      pm.x = pm.y = 0;
      pm.w = pmIcon->w;
      pm.h = pmIcon->h;

      pmscr.x = int(340 * fx);
      pmscr.y = int(270 * fy);
      pmscr.w = pmscr.h = int(100 * fy);

#ifdef SDL2
      SDL_BlitScaled(pmIcon, &pm, screen, &pmscr);
#else
      SDL_SoftStretchOr(pmIcon, &pm, screen, &pmscr);
#endif
      SDL_FreeSurface(pmIcon);
    }
  }
#ifdef SDL2
  FramePresentGL(screen);
#else
  SDL_Flip(screen);
#endif
}

size_t browserVisibleRows()
{
  const double fy = double(g_ScreenHeight) / double(SCREEN_HEIGHT);
  const int top = int(43 * fy);
  const int rowStep = std::max(1, int(20 * fy));
  const int bottomMargin = std::max(1, int(8 * fy));
  if (screen->h <= top + bottomMargin) {
    return 1;
  }
  return std::max(size_t(1), static_cast<size_t>((screen->h - top - bottomMargin) / rowStep));
}

void drawBrowser(const ChooserBackground& background, const std::string& directory, const char *driveLabel,
                 const std::vector<file_entry_t>& entries, size_t selected, size_t first)
{
  const double fx = double(g_ScreenWidth) / double(SCREEN_WIDTH);
  const double fy = double(g_ScreenHeight) / double(SCREEN_HEIGHT);
  if (font_sfc == NULL && !fonts_initialization()) {
    return;
  }
  background.draw();
  // No keyboard-instruction line: keep the header for the drive and current path.
  drawFrame(screen->w, int(55 * fy));
  font_print_centered(screen->w / 2, int(7 * fy), driveLabel, screen, 2.1 * fx, 2.0 * fy);
  font_print_centered(screen->w / 2, int(28 * fy), directory.substr(0, 48).c_str(), screen, 2.1 * fx, 2.0 * fy);

  const int top = int(61 * fy);
  const int rowStep = std::max(1, int(20 * fy));
  const size_t rows = browserVisibleRows();
  const size_t maxNameChars = static_cast<size_t>(std::max(1.0,
      (screen->w - 82.0 * fx) / (FONT_SIZE_X * 2.0 * fx)));
  for (size_t row = 0; row < rows && first + row < entries.size(); ++row) {
    const size_t i = first + row;
    const file_entry_t& entry = entries[i];
    const int y = top + int(row) * rowStep;
    if (i == selected) {
      SDL_Rect r;
      r.x = int(3 * fx);
      r.y = y - int(1 * fy);
      r.w = screen->w - int(6 * fx);
      r.h = int(18 * fy);
      SDL_FillRect(screen, &r, SDL_MapRGB(screen->format, 64, 128, 190));
    }
    const std::string shown = entry.name.substr(0, maxNameChars);
    font_print(int(9 * fx), y, shown.c_str(), screen, 2.0 * fx, 2.0 * fy);
    if (entry.name != kEjectEntryName) {
      font_print(screen->w - int(70 * fx), y, entry.type_or_size_as_string().c_str(), screen, 1.7 * fx, 1.7 * fy);
    }
  }
#ifdef SDL2
  FramePresentGL(screen);
#else
  SDL_Flip(screen);
#endif
}

bool browseForDrive(const ChooserBackground& background, std::string& directory, const char *driveLabel, bool hardDisk,
                    bool mountedMedia, size_t& rememberedIndex, std::string& chosenPath, bool& ejectRequested)
{
  for (;;) {
    ChooserFileList generator(directory, hardDisk);
    std::vector<file_entry_t> entries = generator.generate_file_list();

    if (entries.empty()) {
      return false;
    }

    if (mountedMedia) {
      entries.insert(entries.begin(), file_entry_t(kEjectEntryName, file_entry_t::FILE, 0));
    }

    size_t selected = rememberedIndex < entries.size() ? rememberedIndex : 0;
    const size_t visibleRows = browserVisibleRows();
    size_t first = selected >= visibleRows ? selected - visibleRows + 1 : 0;

#ifdef SDL2
    Uint32 nextScrollTime = 0;
    const Uint32 INITIAL_DELAY = 300;
    const Uint32 REPEAT_RATE = 60;

    for (;;) {
      drawBrowser(background, directory, driveLabel, entries, selected, first);
      const Uint8* state = SDL_GetKeyboardState(NULL);

      if (state[SDL_SCANCODE_UP] || state[SDL_SCANCODE_DOWN] || 
          state[SDL_SCANCODE_LEFT] || state[SDL_SCANCODE_RIGHT]) {
          
          Uint32 now = SDL_GetTicks();
          if (nextScrollTime == 0) {
              nextScrollTime = now + INITIAL_DELAY;
          } else if (now >= nextScrollTime) {
              if (state[SDL_SCANCODE_UP] || state[SDL_SCANCODE_LEFT]) {
                  if (selected > 0) --selected;
              } else if (state[SDL_SCANCODE_DOWN] || state[SDL_SCANCODE_RIGHT]) {
                  if (selected + 1 < entries.size()) ++selected;
              }
              // Keep list aligned during repeats
              if (selected < first) {
                first = selected;
              }
              else if (selected >= first + visibleRows) {
                first = selected - visibleRows + 1;
              }
              nextScrollTime = now + REPEAT_RATE;
          }
      } else {
          nextScrollTime = 0; // Reset when keys are released
      }

      SDL_Event event;
      bool gotKey = false;
      Uint32 checkTimeout = (nextScrollTime != 0) ? 0 : 10; 

      while (!gotKey) {
        SDL_Delay(checkTimeout);
        while (SDL_PollEvent(&event)) {
          if (event.type == SDL_QUIT) {
            return false;
          }
          if (event.type == SDL_KEYDOWN) {
            gotKey = true;
            break;
          }
        }
        if (nextScrollTime != 0 && SDL_GetTicks() >= nextScrollTime) {
            break; 
        }
      }

      if (gotKey) {
        const SDL_Keycode key = event.key.keysym.sym;
  
        if (key == SDLK_ESCAPE) {
          return false;
        }
  
        nextScrollTime = SDL_GetTicks() + INITIAL_DELAY; 
  
        if (key == SDLK_UP || key == SDLK_LEFT) {
          if (selected > 0) {
            --selected;
          }
        } else if (key == SDLK_DOWN || key == SDLK_RIGHT) {
          if (selected + 1 < entries.size()) {
            ++selected;
          }
        } else if (key == SDLK_PAGEUP) {
          selected = selected > visibleRows ? selected - visibleRows : 0;
        } else if (key == SDLK_PAGEDOWN) {
          selected = std::min(selected + visibleRows, entries.size() - 1);
        } else if (key == SDLK_HOME) {
          selected = 0;
        } else if (key == SDLK_END) {
          selected = entries.size() - 1;
        } else if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE) {
          const file_entry_t& entry = entries[selected];
          rememberedIndex = selected;
          if (entry.name == kEjectEntryName) {
            ejectRequested = true;
            rememberedIndex = 0;
            return true;
          }
          if (entry.is_dir_type()) {
            if (entry.type == file_entry_t::UP || entry.name == "..") {
              directory = parentPath(directory);
            }
            else if (entry.name != ".") {
              directory = joinPath(directory, entry.name);
            }
            break; // rescan the newly selected directory
          }
          chosenPath = joinPath(directory, entry.name);
          return true;
        }
        if (selected < first) {
          first = selected;
        }
        else if (selected >= first + visibleRows) {
          first = selected - visibleRows + 1;
        }
      }
    }

#else

    for (;;) {
      drawBrowser(background, directory, driveLabel, entries, selected, first);
      SDL_Event event;
      bool gotKey = false;
      while (!gotKey) {
        SDL_Delay(10);
        while (SDL_PollEvent(&event)) {
          if (event.type == SDL_QUIT) {
            return false;
          }
          if (event.type == SDL_KEYDOWN) {
            gotKey = true;
            break;
          }
        }
      }

      const SDLKey key = event.key.keysym.sym;

      if (key == SDLK_ESCAPE) {
        return false;
      }

      if (key == SDLK_UP || key == SDLK_LEFT) {
        if (selected > 0) {
          --selected;
        }
      } else if (key == SDLK_DOWN || key == SDLK_RIGHT) {
        if (selected + 1 < entries.size()) {
          ++selected;
        }
      } else if (key == SDLK_PAGEUP) {
        selected = selected > visibleRows ? selected - visibleRows : 0;
      } else if (key == SDLK_PAGEDOWN) {
        selected = std::min(selected + visibleRows, entries.size() - 1);
      } else if (key == SDLK_HOME) {
        selected = 0;
      } else if (key == SDLK_END) {
        selected = entries.size() - 1;
      } else if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE) {
        const file_entry_t& entry = entries[selected];
        rememberedIndex = selected;
        if (entry.name == kEjectEntryName) {
          ejectRequested = true;
          rememberedIndex = 0;
          return true;
        }
        if (entry.is_dir_type()) {
          if (entry.type == file_entry_t::UP || entry.name == "..") {
            directory = parentPath(directory);
          }
          else if (entry.name != ".") {
            directory = joinPath(directory, entry.name);
          }
          break; // rescan the newly selected directory
        }
        chosenPath = joinPath(directory, entry.name);
        return true;
      }
      if (selected < first) {
        first = selected;
      }
      else if (selected >= first + visibleRows) {
        first = selected - visibleRows + 1;
      }
    }
#endif

  }
}

} // namespace

bool ChooserRun(const std::string& startDirectory)
{
  std::string directories[4] = { startDirectory, startDirectory, startDirectory, startDirectory };
  size_t indices[4] = { 0, 0, 0, 0 };
  int selected = 0;
  ChooserBackground background;

  for (;;) {
    drawMenu(background, selected);
    SDL_Event event;
    bool gotKey = false;
    while (!gotKey) {
      SDL_Delay(10);
      while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
          return false;
        }
        if (event.type == SDL_KEYDOWN) {
          gotKey = true;
          break;
        }
      }
    }
#ifdef SDL2
    const SDL_Keycode key = event.key.keysym.sym;

    if (key == SDLK_ESCAPE) {
      return false;
    }

    if (key == SDLK_UP || key == SDLK_LEFT) {
      selected = (selected + 4) % 5;
    }
    else if (key == SDLK_DOWN || key == SDLK_RIGHT) {
      selected = (selected + 1) % 5;
    }
    else if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE) {
      if (selected == 4) {
        setupAndLaunchGptokeyb();
        return true;
      }

      for (;;) {
        std::string fullPath;
        bool ejectRequested = false;
        const char *driveLabels[4] = { "Select Media for 5.25\" Drive 1", "Select Media for 5.25\" Drive 2", "Select Media for Hard Disk Drive 1", "Select Media for Hard Disk Drive 2" };
        const bool mountedMedia = selected < 2
          ? hasMountedMedia(DiskGetFullName(selected))
          : hasMountedMedia(HD_GetFullName(selected - 2));
        if (!browseForDrive(background, directories[selected], driveLabels[selected], selected >= 2,
                            mountedMedia, indices[selected], fullPath, ejectRequested))
        {
          break;
        }
        if (ejectRequested) {
          if (selected < 2) {
            DiskEject(selected);
          } else {
            HD_Eject(selected - 2);
          }
          break;
        }
        if (selected < 2) {
          if (DoDiskInsert(selected, const_cast<char*>(fullPath.c_str())) != 0) {
            break;
          }
        } else {
          if (!HD_InsertDisk2(selected - 2, fullPath.c_str())) {
            break;
          }
          hddenabled = true;
          HD_SetEnabled(true);
        }
        break;
      }
    }

#else
    const SDLKey key = event.key.keysym.sym;

    if (key == SDLK_ESCAPE) {
      return false;
    }

    if (key == SDLK_UP || key == SDLK_LEFT) {
      selected = (selected + 4) % 5;
    }
    else if (key == SDLK_DOWN || key == SDLK_RIGHT) {
      selected = (selected + 1) % 5;
    }
    else if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE) {
      if (selected == 4) {
        setupAndLaunchGptokeyb();
        return true;
      }

      for (;;) {
        std::string fullPath;
        bool ejectRequested = false;
        const char *driveLabels[4] = { "Select Media for 5.25\" Drive 1", "Select Media for 5.25\" Drive 2", "Select Media for Hard Disk Drive 1", "Select Media for Hard Disk Drive 2" };
        const bool mountedMedia = selected < 2
          ? hasMountedMedia(DiskGetFullName(selected))
          : hasMountedMedia(HD_GetFullName(selected - 2));
        if (!browseForDrive(background, directories[selected], driveLabels[selected], selected >= 2,
                            mountedMedia, indices[selected], fullPath, ejectRequested))
        {
          break;
        }
        if (ejectRequested) {
          if (selected < 2) {
            DiskEject(selected);
          } else {
            HD_Eject(selected - 2);
          }
          break;
        }
        if (selected < 2) {
          if (DoDiskInsert(selected, const_cast<char*>(fullPath.c_str())) != 0) {
            break;
          }
        } else {
          if (!HD_InsertDisk2(selected - 2, fullPath.c_str())) {
            break;
          }
          hddenabled = true;
          HD_SetEnabled(true);
        }
        break;
      }
    }
#endif
  }
}
