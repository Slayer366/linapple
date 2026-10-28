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

/* Description: Hard drive emulation
 *
 * Author: Copyright (c) 2005, Robert Hoem
 */

/* Adaptation for SDL and POSIX (l) by beom beotiger, Nov-Dec 2007 */

#include "stdafx.h"
#include "wwrapper.h"
#include "ftpparse.h"
#include "DiskFTP.h"

/*
Memory map:

    C0F0  (r)   EXECUTE AND RETURN STATUS
  C0F1  (r)   STATUS (or ERROR)
  C0F2  (r/w) COMMAND
  C0F3  (r/w) UNIT NUMBER
  C0F4  (r/w) LOW unsigned char OF MEMORY BUFFER
  C0F5  (r/w) HIGH unsigned char OF MEMORY BUFFER
  C0F6  (r/w) LOW unsigned char OF BLOCK NUMBER
  C0F7  (r/w) HIGH unsigned char OF BLOCK NUMBER
  C0F8    (r)   NEXT unsigned char
*/

/*
Hard drive emulation in Applewin.

Concept
    To emulate a 32mb hard drive connected to an Apple IIe via Applewin.
    Designed to work with Autoboot Rom and Prodos.

Overview
  1. Hard drive image file
      The hard drive image file (.HDV) will be formatted into blocks of 512
      bytes, in a linear fashion. The internal formatting and meaning of each
      block to be decided by the Apple's operating system (ProDos). To create
      an empty .HDV file, just create a 0 byte file (I prefer the debug method).

  2. Emulation code
      There are 4 commands Prodos will send to a block device.
      Listed below are each command and how it's handled:

      1. STATUS
          In the emulation's case, returns only a DEVICE OK (0) or DEVICE I/O ERROR (8).
          DEVICE I/O ERROR only returned if no HDV file is selected.

      2. READ
          Loads requested block into a 512 byte buffer by attempting to seek to
            location in HDV file.
          If seek fails, returns a DEVICE I/O ERROR.  Resets hd_buf_ptr used by HD_NEXTBYTE
          Returns a DEVICE OK if read was successful, or a DEVICE I/O ERROR otherwise.

      3. WRITE
          Copies requested block from the Apple's memory to a 512 byte buffer
            then attempts to seek to requested block.
          If the seek fails (usually because the seek is beyond the EOF for the
            HDV file), the Emulation will attempt to "grow" the HDV file to accomodate.
            Once the file can accomodate, or if the seek did not fail, the buffer is
            written to the HDV file.  NOTE: A2PC will grow *AND* shrink the HDV file.
          I didn't see the point in shrinking the file as this behaviour would require
            patching prodos (to detect DELETE FILE calls).

      4. FORMAT
          Ignored.  This would be used for low level formatting of the device
            (as in the case of a tape or SCSI drive, perhaps).

  3. Bugs
      The only thing I've noticed is that Copy II+ 7.1 seems to crash or stall
      occasionally when trying to calculate how many free block are available
      when running a catalog.  This might be due to the great number of blocks
      available.  Also, DDD pro will not optimise the disk correctally (it's
      doing a disk defragment of some sort, and when it requests a block outside
      the range of the image file, it starts getting I/O errors), so don't
      bother.  Any program that preforms a read before write to an "unwritten"
      block (a block that should be located beyond the EOF of the .HDV, which is
      valid for writing but not for reading until written to) will fail with I/O
      errors (although these are few and far between).

      I'm sure there are programs out there that may try to use the I/O ports in
      ways they weren't designed (like telling Ultima 5 that you have a Phazor
      sound card in slot 7 is a generally bad idea) will cause problems.
*/

static const unsigned char Hddrvr_dat[256] = {
    0xA9, 0x20, 0xA9, 0x00, 0xA9, 0x03, 0xA9, 0x3C, 0xA9, 0xC0, 0x85, 0x47,
    0x0A, 0x0A, 0x0A, 0x0A, 0x85, 0x43, 0xA5, 0x47, 0x48, 0xA9, 0x1D, 0x48,
    0xA9, 0x00, 0x85, 0x42, 0xF0, 0x28, 0xB0, 0x1F, 0xA5, 0x47, 0x48, 0xA9,
    0x37, 0x48, 0xA9, 0x00, 0x85, 0x44, 0x85, 0x46, 0x85, 0x47, 0xA9, 0x08,
    0x85, 0x45, 0xA9, 0x01, 0x85, 0x42, 0xD0, 0x0E, 0xB0, 0x05, 0xA6, 0x43,
    0x4C, 0x01, 0x08, 0xB0, 0x6E, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA5, 0x43,
    0x29, 0x70, 0xAA, 0xA5, 0x43, 0x9D, 0x81, 0xC0, 0xA5, 0x46, 0x9D, 0x82,
    0xC0, 0xA5, 0x47, 0x9D, 0x83, 0xC0, 0xA5, 0x42, 0xC9, 0x02, 0xD0, 0x18,
    0xA0, 0x00, 0xB1, 0x44, 0x9D, 0x84, 0xC0, 0xC8, 0xD0, 0xF8, 0xE6, 0x45,
    0xB1, 0x44, 0x9D, 0x84, 0xC0, 0xC8, 0xD0, 0xF8, 0xC6, 0x45, 0xA5, 0x42,
    0x9D, 0x80, 0xC0, 0xBD, 0x80, 0xC0, 0xD0, 0x2D, 0xA5, 0x42, 0xF0, 0x1E,
    0xC9, 0x01, 0xD0, 0x16, 0xA0, 0x00, 0xBD, 0x84, 0xC0, 0x91, 0x44, 0xC8,
    0xD0, 0xF8, 0xE6, 0x45, 0xBD, 0x84, 0xC0, 0x91, 0x44, 0xC8, 0xD0, 0xF8,
    0xC6, 0x45, 0xA9, 0x00, 0x18, 0x60, 0xBC, 0x86, 0xC0, 0xBD, 0x85, 0xC0,
    0xAA, 0xA9, 0x00, 0x18, 0x60, 0x38, 0x60, 0xAD, 0xB3, 0xFB, 0xC9, 0x38,
    0xF0, 0x11, 0xA5, 0x43, 0x4A, 0x4A, 0x4A, 0x4A, 0x09, 0xC0, 0x85, 0x01,
    0xA9, 0x00, 0x85, 0x00, 0x4C, 0xBA, 0xFA, 0x4C, 0x59, 0xFF, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xDF, 0x46
};


typedef struct {
  char hd_imagename[16];
  char hd_fullname[128];
  char hd_imagepath[MAX_PATH];
  unsigned char hd_error;
  unsigned short hd_memblock;
  unsigned short hd_diskblock;
  unsigned short hd_buf_ptr;
  bool hd_imageloaded;
  HANDLE hd_file;
  unsigned int hd_data_offset;
  unsigned int hd_data_length; // zero for legacy raw images, which may grow
  bool hd_read_only;
  unsigned char hd_buf[513];
} HDD, *PHDD;

static bool g_bHD_RomLoaded = false;
bool g_bHD_Enabled = false;

static unsigned char g_nHD_UnitNum = DRIVE_1;

// The HDD interface has a single Command register for both drives:
// . ProDOS will write to Command before switching drives
static unsigned char g_nHD_Command;

static HDD g_HardDrive[2] = {0};

static unsigned int g_uSlot = 7;

static int HDDStatus = DISK_STATUS_OFF;  // status: 0 - none, 1 - read, 2 - write

int HD_GetStatus(void)
{
  int result = HDDStatus;
  return result;
}

void HD_ResetStatus(void)
{
  HDDStatus = DISK_STATUS_OFF;
}

static void HD_SetStatus(int status)
{
  if (HDDStatus != status) {
    HDDStatus = status;
    FrameRefreshStatus(DRAW_LEDS);
  }
}

static void GetImageTitle(LPCTSTR imageFileName, PHDD pHardDrive)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
  char imagetitle[128];
  LPCTSTR startpos = imageFileName;

  if (_tcsrchr(startpos, FILE_SEPARATOR)) {
    startpos = _tcsrchr(startpos, FILE_SEPARATOR) + 1;
  }
  _tcsncpy(imagetitle, startpos, 127);
  imagetitle[127] = 0;

  bool found = 0;
  int loop = 0;
  while (imagetitle[loop] && !found) {
    if (IsCharLower(imagetitle[loop])) {
      found = 1;
    } else {
      loop++;
    }
  }

  _tcsncpy(pHardDrive->hd_fullname, imagetitle, 127);
  pHardDrive->hd_fullname[127] = 0;

  if (imagetitle[0]) {
    LPTSTR dot = imagetitle;
    if (_tcsrchr(dot, TEXT('.'))) {
      dot = _tcsrchr(dot, TEXT('.'));
    }
    if (dot > imagetitle) {
      *dot = 0;
    }
  }

  _tcsncpy(pHardDrive->hd_imagename, imagetitle, 15);
  pHardDrive->hd_imagename[15] = 0;
#pragma GCC diagnostic pop
}

static void NotifyInvalidImage(char *filename)
{
  printf("HDD: Could not load %s\n", filename);
}

static void HD_CleanupDrive(int nDrive)
{
  if (g_HardDrive[nDrive].hd_file) {
    CloseHandle(g_HardDrive[nDrive].hd_file);
    g_HardDrive[nDrive].hd_file = NULL;
  }
  g_HardDrive[nDrive].hd_imageloaded = false;
  g_HardDrive[nDrive].hd_data_offset = 0;
  g_HardDrive[nDrive].hd_data_length = 0;
  g_HardDrive[nDrive].hd_read_only = false;
  g_HardDrive[nDrive].hd_imagename[0] = 0;
  g_HardDrive[nDrive].hd_fullname[0] = 0;
  g_HardDrive[nDrive].hd_imagepath[0] = 0;
}

static unsigned int ReadLE32(const unsigned char *p)
{
  return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
         ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

static unsigned int ReadLE16(const unsigned char *p)
{
  return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static bool HD_Load_Image(int nDrive, LPCSTR filename)
{
//  g_HardDrive[nDrive].hd_file = fopen(filename, "r+b");
  HDD *pHDD = &g_HardDrive[nDrive];
  unsigned int dataOffset = 0;
  unsigned int dataLength = 0;
  bool readOnly = false;
  unsigned char header[64];
  FILE *probe = fopen(filename, "rb");

//  if (g_HardDrive[nDrive].hd_file == INVALID_HANDLE_VALUE) {
//    g_HardDrive[nDrive].hd_imageloaded = false;
//  } else {
//    g_HardDrive[nDrive].hd_imageloaded = true;
//  }

  if (!probe) {
    return false;
  }

  size_t headerBytes = fread(header, 1, sizeof(header), probe);
  if (headerBytes >= 4 && memcmp(header, "2IMG", 4) == 0) {
    // A hard-disk 2MG is a ProDOS-order block image. The block data begins
    // at dataOffset; the 64-byte 2IMG header is not part of the disk.
    if (headerBytes < sizeof(header) || ReadLE16(header + 8) < 64 ||
        ReadLE16(header + 8) > GetFileSize((HANDLE)probe, NULL) ||
        ReadLE32(header + 12) != 1) {
      fclose(probe);
      return false;
    }

    dataOffset = ReadLE32(header + 24);
    dataLength = ReadLE32(header + 28);
    if (dataOffset == 0)
      dataOffset = ReadLE16(header + 8);

    unsigned int fileSize = GetFileSize((HANDLE)probe, NULL);
    unsigned int blockCount = ReadLE32(header + 20);
    if (dataLength == 0 && blockCount != 0 && blockCount <= 0xFFFFFFFFu / 512u) {
      dataLength = blockCount * 512u;
    }
    if (dataOffset < ReadLE16(header + 8) || dataOffset > fileSize || dataLength == 0 ||
        dataLength > fileSize - dataOffset || (dataLength % 512) != 0 ||
        (blockCount != 0 && blockCount > 0xFFFFFFFFu / 512u) ||
        (blockCount != 0 && blockCount * 512u != dataLength)) {
      fclose(probe);
      return false;
    }

    readOnly = (ReadLE32(header + 16) & 0x80000000u) != 0;
   }
  fclose(probe);

  pHDD->hd_file = fopen(filename, readOnly ? "rb" : "r+b");
  if (!pHDD->hd_file) {
    return false;
  }

//  return g_HardDrive[nDrive].hd_imageloaded;
  pHDD->hd_data_offset = dataOffset;
  pHDD->hd_data_length = dataLength;
  pHDD->hd_read_only = readOnly;
  pHDD->hd_imageloaded = true;
  return true;
}

#if 0
static LPCTSTR HD_DiskGetName (int nDrive)
{
  return g_HardDrive[nDrive].hd_imagename;
}
#endif

// Everything below is global

static unsigned char HD_IO_EMUL(unsigned short pc, unsigned short addr, unsigned char bWrite, unsigned char d, ULONG nCyclesLeft);

static const unsigned int HDDRVR_SIZE = 0x100;

bool HD_CardIsEnabled() {
  return g_bHD_RomLoaded && g_bHD_Enabled;
}

void HD_SetEnabled(bool bEnabled) {
  if (g_bHD_Enabled == bEnabled)
    return;

  g_bHD_Enabled = bEnabled;

  LPBYTE pCxRomPeripheral = MemGetCxRomPeripheral();
  if (pCxRomPeripheral == NULL) { // This will be NULL when called after loading value from Registry
    return;
  }

  if (g_bHD_Enabled) {
    HD_Load_Rom(pCxRomPeripheral, g_uSlot);
  } else {
    memset(pCxRomPeripheral + g_uSlot * 256, 0, HDDRVR_SIZE);
  }

  RegisterIoHandler(g_uSlot, HD_IO_EMUL, HD_IO_EMUL, NULL, NULL, NULL, NULL);
}

LPCTSTR HD_GetFullName(int nDrive) {
  return g_HardDrive[nDrive].hd_fullname;
}

LPCTSTR HD_GetImagePath(int nDrive) {
  if (nDrive < 0 || nDrive >= 2)
    return "";

  return g_HardDrive[nDrive].hd_imagepath;
}

void HD_Load_Rom(LPBYTE pCxRomPeripheral, unsigned int uSlot) {
  if (!g_bHD_Enabled) {
    return;
  }

  unsigned char *pData = (unsigned char *) Hddrvr_dat;  // NB. Don't need to unlock resource - hmmmmmm....... i love linux

  g_uSlot = uSlot;
  memcpy(pCxRomPeripheral + uSlot * 256, pData, HDDRVR_SIZE);
  pCxRomPeripheral[uSlot * 256 + 0x09] = (unsigned char)(0xC0 | (uSlot & 0x0F));
  g_bHD_RomLoaded = true;
}

void HD_Cleanup() {
  for (int i = DRIVE_1; i < DRIVE_2; i++) {
    HD_CleanupDrive(i);
  }
}

void HD_Eject(const int iDrive) {
  if (g_HardDrive[iDrive].hd_imageloaded) {
    HD_CleanupDrive(iDrive);
    if (iDrive == 0) {
      RegSaveString(TEXT("Preferences"), REGVALUE_HDD_IMAGE1, 1, "");
    } else {
      RegSaveString(TEXT("Preferences"), REGVALUE_HDD_IMAGE2, 1, "");
    }
  }
}

// pszFilename is not qualified with path
bool HD_InsertDisk2(int nDrive, LPCTSTR pszFilename) {
  if (*pszFilename == 0x00) {
    return false;
  }
  return HD_InsertDisk(nDrive, pszFilename);
}

// imageFileName is qualified with path
bool HD_InsertDisk(int nDrive, LPCTSTR imageFileName) {
  if (*imageFileName == 0x00) {
    return false;
  }

  if (g_HardDrive[nDrive].hd_imageloaded) {
    HD_CleanupDrive(nDrive);
  }

  bool result = HD_Load_Image(nDrive, imageFileName);

  if (result) {
    GetImageTitle(imageFileName, &g_HardDrive[nDrive]);
    strncpy(g_HardDrive[nDrive].hd_imagepath, imageFileName, MAX_PATH - 1);
    g_HardDrive[nDrive].hd_imagepath[MAX_PATH - 1] = 0;
  } else {
    NotifyInvalidImage((char *) imageFileName);  // could not load hd image
  }

  return result;
}

void HD_FTP_Select(int nDrive)
{
  // Selects HDrive from FTP directory
  static size_t fileIndex = 0; // file index will be remembered for current dir
  static size_t backdx = 0;  //reserve
  static size_t dirdx = 0;  // reserve for dirs

  std::string filename;      // given filename
  std::string fullPath;  // full path for it
  bool isDirectory = true;      // if given filename is a directory?

  fileIndex = backdx;
  fullPath = g_sFTPServerHDD;  // global var for FTP path for HDD

  while (isDirectory) {
    if (!ChooseAnImageFTP(g_ScreenWidth, g_ScreenHeight, fullPath, 7,
                          filename, isDirectory, fileIndex)) {
      DrawFrameWindow();
      return;  // if ESC was pressed, just leave
    }
    // --
    if (isDirectory) {
      if (filename == "..") {
        // go to the upper directory
        auto r = fullPath.find_last_of(FTP_SEPARATOR);
        if (r == fullPath.size()-1) {
          r = fullPath.find_last_of(FTP_SEPARATOR, r-1);
        }
        if (r != std::string::npos) {
          fullPath = fullPath.substr(0, 1+r);
        }
        if (fullPath == "") {
          fullPath = "/";  //we don't want fullPath to be empty
        }
        fileIndex = dirdx;  // restore
      } else {
        if (fullPath != "/") {
          fullPath += filename + "/";
        } else {
          fullPath = "/" + filename + "/";
        }
        printf("HD_FTP_Select: we build %s\n", fullPath.c_str());
        dirdx = fileIndex; // store it
        fileIndex = 0;  // start with beginning of dir
      }
    }
  }
  // we chose some file
  strcpy(g_sFTPServerHDD, fullPath.c_str());
  RegSaveString(TEXT("Preferences"), REGVALUE_FTP_HDD_DIR, 1, g_sFTPServerHDD);// save it

  fullPath += "/" + filename;

  std::string localPath = std::string(g_sFTPLocalDir) + "/" + filename; // local path for file

  int error = ftp_get(fullPath.c_str(), localPath.c_str());
  if (!error) {
    if (HD_InsertDisk2(nDrive, localPath.c_str())) {
      // save file names for HDD disk 1 or 2
      if (nDrive) {
        RegSaveString(TEXT("Preferences"), REGVALUE_HDD_IMAGE2, 1, localPath.c_str());
      } else {
        RegSaveString(TEXT("Preferences"), REGVALUE_HDD_IMAGE1, 1, localPath.c_str());
      }
    }
  }
  backdx = fileIndex;  //store cursor position
  DrawFrameWindow();
}

void HD_Select(int nDrive)
{
  // Selects HDrive from file list
  static size_t fileIndex = 0; // file index will be remembered for current dir
  static size_t backdx = 0;  //reserve
  static size_t dirdx = 0;  // reserve for dirs

  std::string filename;      // given filename
  std::string fullPath;  // full path for it
  bool isDirectory;      // if given filename is a directory?

  fileIndex = backdx;
  isDirectory = true;
  fullPath = g_sHDDDir;  // global var for disk selecting directory

  while (isDirectory) {
    if (!ChooseAnImage(g_ScreenWidth, g_ScreenHeight, fullPath, 7,
                       filename, isDirectory, fileIndex)) {
      DrawFrameWindow();
      return;  // if ESC was pressed, just leave
    }
    if (isDirectory) {
      if (filename == "..") { // go to the upper directory
        const auto last_sep_pos = fullPath.find_last_of(FILE_SEPARATOR);
        if (last_sep_pos != std::string::npos) {
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
  // we chose some file
  strcpy(g_sHDDDir, fullPath.c_str());
  RegSaveString(TEXT("Preferences"), REGVALUE_PREF_HDD_START_DIR, 1, g_sHDDDir); // Save it

  fullPath += "/" + filename;

  // in future: save file name in registry for future fetching
  // for one drive will be one reg parameter
  if (HD_InsertDisk2(nDrive, fullPath.c_str())) {
    // save file names for HDD disk 1 or 2
    if (nDrive) {
      RegSaveString(TEXT("Preferences"), REGVALUE_HDD_IMAGE2, 1, fullPath.c_str());
    } else {
      RegSaveString(TEXT("Preferences"), REGVALUE_HDD_IMAGE1, 1, fullPath.c_str());
    }
    printf("HDD disk image %s inserted\n", fullPath.c_str());
  }
  backdx = fileIndex; // Store cursor position
  DrawFrameWindow();
}

static unsigned char HD_IO_EMUL(unsigned short pc, unsigned short addr, unsigned char bWrite, unsigned char d, ULONG nCyclesLeft) {
  unsigned char r = 0;
  addr &= 0x0F;

  if (!HD_CardIsEnabled()) {
    return r;
  }

  PHDD pHDD = &g_HardDrive[g_nHD_UnitNum >> 7];  // bit7 = drive select

  if (bWrite == 0) { // Read from HW Register
    switch (addr) {
      case 0x0: // Command Register
        r = pHDD->hd_error;
        break;
      case 0x1: // Unit Latch
        r = g_nHD_UnitNum;
        break;
      case 0x2: // Block Register Low
        r = (unsigned char)(pHDD->hd_diskblock & 0x00FF);
        break;
      case 0x3: // Block Register High
        r = (unsigned char)((pHDD->hd_diskblock & 0xFF00) >> 8);
        break;
      case 0x4: // Data Stream Port
        r = pHDD->hd_buf[pHDD->hd_buf_ptr];
        pHDD->hd_buf_ptr = (pHDD->hd_buf_ptr + 1) % 512;
        break;
      case 0x5: // Block Count Low (for STATUS commands)
        r = (unsigned char)(pHDD->hd_memblock & 0x00FF); // We store block count here now
        break;
      case 0x6: // Block Count High
        r = (unsigned char)((pHDD->hd_memblock & 0xFF00) >> 8);
        break;
      default:
        r = 0xFF; // Open floating bus fallback (No wheels and hovers?)
        break;
    }
  } else { // Write to HW Register
    switch (addr) {
      case 0x0: // Command Execution
        g_nHD_Command = d;

        if (!pHDD->hd_imageloaded) {
          pHDD->hd_error = 0x28; // no device error
          break;
        }

        if (g_nHD_Command == 0x00) { // Stat Command
          unsigned int totalBlocks = pHDD->hd_data_length ? (pHDD->hd_data_length / 512) : (GetFileSize(pHDD->hd_file, NULL) / 512);
          if (totalBlocks > 0xFFFF) totalBlocks = 0xFFFF;
          pHDD->hd_memblock = (unsigned short)totalBlocks; // Feed back to block count registers
          pHDD->hd_error = 0x00; // Success
        }
        else if (g_nHD_Command == 0x01) { // Read Command
          HD_SetStatus(DISK_STATUS_READ);
          unsigned int blockOffset = (unsigned int)pHDD->hd_diskblock * 512u;
          SetFilePointer(pHDD->hd_file, (int)(pHDD->hd_data_offset + blockOffset), NULL, FILE_BEGIN);
          unsigned int br;
          if (ReadFile(pHDD->hd_file, pHDD->hd_buf, 512, &br, NULL)) {
            pHDD->hd_error = 0x00;
            pHDD->hd_buf_ptr = 0;
          } else {
            pHDD->hd_error = 0x27; // IO error
          }
        }
        else if (g_nHD_Command == 0x02) { // Write Command
          HD_SetStatus(DISK_STATUS_WRITE);
          if (pHDD->hd_read_only) {
            pHDD->hd_error = 0x2B; // Write protected
          } else {
            unsigned int blockOffset = (unsigned int)pHDD->hd_diskblock * 512u;
            SetFilePointer(pHDD->hd_file, (int)(pHDD->hd_data_offset + blockOffset), NULL, FILE_BEGIN);
            unsigned int bw;
            if (WriteFile(pHDD->hd_file, pHDD->hd_buf, 512, &bw, NULL)) {
              pHDD->hd_error = 0x00;
            } else {
              pHDD->hd_error = 0x27; // IO error
            }
          }
        }
        else {
          pHDD->hd_error = 0x00; // FORMAT command fallback success
        }
        break;

      case 0x1: // Unit Latch
        g_nHD_UnitNum = d;
        pHDD = &g_HardDrive[g_nHD_UnitNum >> 7]; // Remap drive pointer focus immediately
        pHDD->hd_buf_ptr = 0;
        break;
      case 0x2: // Block Low
        pHDD->hd_diskblock = (pHDD->hd_diskblock & 0xFF00) | d;
        break;
      case 0x3: // Block High
        pHDD->hd_diskblock = (pHDD->hd_diskblock & 0x00FF) | ((unsigned int)d << 8);
        break;
      case 0x4: // Data Stream Push Port
        pHDD->hd_buf[pHDD->hd_buf_ptr] = d;
        pHDD->hd_buf_ptr = (pHDD->hd_buf_ptr + 1) % 512;
        break;
    }
  }
//  FrameRefreshStatus(DRAW_LEDS);  // update status area
  return r;
}
