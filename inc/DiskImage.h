#pragma once

#define  NIBBLES                6656
#define  NIBBLES_PER_TRACK      0x1A00
#define  TRACK_DENIBBLIZED_SIZE (16 * 256)  // #Sectors x Sector-size

#define  TRACKS       35  // Standard 35 track count
#define	 TRACKS_EXTRA 5		// Allow up to a 40-track .dsk image (160KB)
#define	 TRACKS_MAX   (TRACKS+TRACKS_EXTRA)

#define  IMAGETYPES  8

#define  WOZ2_HEADER_SIZE     1536 /* three 512-byte blocks */
#define  WOZ2_DATA_BLOCK_SIZE  512
#define  WOZ2_TMAP_OFFSET       88 /* bytes from beginning of WOZ2 file */
#define  WOZ2_TMAP_SIZE        160
#define  WOZ2_TRKS_OFFSET      256 /* bytes from beginning of WOZ2 file */
#define  WOZ2_TRKS_MAX_SIZE    160
#define  WOZ2_TRK_SIZE           8
#define  WOZ1_HEADER_SIZE      256
#define  WOZ1_TRACK_SIZE      6656
#define  WOZ1_TRACK_DATA_SIZE 6646
#define  WOZ1_TMAP_OFFSET       88 /* bytes from beginning of WOZ1 file */

bool ImageBoot(HIMAGE);
void ImageClose(HIMAGE);

void ImageDestroy();

void ImageInitialize();

enum ImageError_e {
  IMAGE_ERROR_BAD_POINTER = -1, IMAGE_ERROR_NONE = 0, IMAGE_ERROR_UNABLE_TO_OPEN = 1, IMAGE_ERROR_BAD_SIZE = 2
};

int ImageOpen(LPCTSTR imagefilename, HIMAGE *hDiskImage_, bool *pWriteProtected_, bool bCreateIfNecessary);

void ImageReadTrack(HIMAGE, int, int, LPBYTE, int *);

void ImageWriteTrack(HIMAGE, int, int, LPBYTE, int);
