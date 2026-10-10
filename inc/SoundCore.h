#pragma once

#define MAX_SAMPLES (16*1024)

#ifndef INFINITE
#define INFINITE 0xFFFFFFFF  // Infinite time-out
#endif

#ifndef WAIT_OBJECT_0
#define WAIT_OBJECT_0 ((STATUS_WAIT_0) + 0)
#endif

typedef struct {
   bool bActive;			// Playback is active
   bool bMute;
   long nVolume;			// Current volume
} VOICE;

// Max volume for disable sound distortion
#define SD_VOLUME  SDL_MIX_MAXVOLUME / 2
enum {
  FADE_OUT, FADE_IN
};

bool DSInit();    // init SDL_Auidio
void DSUninit();  // uninit SDL_Auidio

void SoundCore_SetFade(int how);  //

void DSUploadBuffer(short *buffer, unsigned len);

void DSUploadMockBuffer(short *buffer, unsigned len);  // Upload Mockingboard data

void DSSpeechStart(short *buffer, unsigned numSamples);
void DSSpeechStopAndReset();
bool DSSpeechIsActive();
void DSSpeechFinished();
unsigned int DSSpeechGetFreeSpace();
void SSI263_UpdateCycles(unsigned int cycles);

extern bool g_bDSAvailable;
