/*
 * This file is part of Espruino, a JavaScript interpreter for Microcontrollers
 *
 * Copyright (C) 2024 Gordon Williams <gw@pur3.co.uk>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * ----------------------------------------------------------------------------
 *
 * AVI/WAV file decode
 *
 * ---------------------------------------------------------------------------- */
#include "jsutils.h"
#include "jsinteractive.h"
#include "avi.h"

typedef struct {
  uint32_t dwMicroSecPerFrame; // frame display rate (or 0)
  uint32_t dwMaxBytesPerSec; // max. transfer rate
  uint32_t dwPaddingGranularity; // pad to multiples of this size
  uint32_t dwFlags; // the ever-present flags
  uint32_t dwTotalFrames; // # frames in file
  uint32_t dwInitialFrames;
  uint32_t dwStreams;
  uint32_t dwSuggestedBufferSize;
  uint32_t dwWidth;
  uint32_t dwHeight;
  uint32_t dwReserved[4];
} PACKED_FLAGS MainAVIHeader;

typedef struct {
  uint32_t fccType;
  uint32_t fccHandler;
  uint32_t dwFlags;
  uint16_t wPriority;
  uint16_t wLanguage;
  uint32_t dwInitialFrames;
  uint32_t dwScale;
  uint32_t dwRate; /* dwRate / dwScale == samples/second */
  uint32_t dwStart;
  uint32_t dwLength; /* In units above... */
  uint32_t dwSuggestedBufferSize;
  uint32_t dwQuality;
  uint32_t dwSampleSize;
  struct
  {
    uint16_t Left;
    uint16_t Top;
    uint16_t Right;
    uint16_t Bottom;
  } PACKED_FLAGS  rcFrame;
} PACKED_FLAGS AVIStreamHeader;

typedef struct {
  uint32_t biSize;
  uint32_t biWidth;
  uint32_t biHeight;
  uint16_t biPlanes;
  uint16_t biBitCount;
  uint32_t biCompression;
  uint32_t biSizeImage;
  uint32_t biXPelsPerMeter;
  uint32_t biYPelsPerMeter;
  uint32_t biClrUsed;
  uint32_t biClrImportant;
} PACKED_FLAGS BITMAPINFOHEADER;

typedef struct {
  uint16_t formatTag;
  uint16_t channels;
  uint32_t sampleRate;
  uint32_t baudRate;
  uint16_t blockAlign;
  uint16_t size;
} PACKED_FLAGS WAVHEADER;

bool is4CC(uint8_t *ptr, const char *fourcc) {
  return
    ptr[0]==fourcc[0] &&
    ptr[1]==fourcc[1] &&
    ptr[2]==fourcc[2] &&
    ptr[3]==fourcc[3];
}
uint32_t getU32(uint8_t *buf) {
  return *(uint32_t*)buf; // little endian
}
// find the entry in the list, or return 0. Returns the offset of the data (so -4=size)
uint8_t *riffListFind(uint8_t *buf, const char *fourcc) {
  // FIXME: these should all really be bounds-checked
  assert(is4CC(buf, "LIST")||is4CC(buf, "RIFF"));
  uint32_t listEnd = getU32(&buf[4])+8;
  uint32_t offs=12;
  while (offs<listEnd) {
    if ((is4CC(&buf[offs], "LIST") && is4CC(&buf[offs+8], fourcc)) ||
        is4CC(&buf[offs], fourcc)) {
      return &buf[offs+8];
    }
    offs += (getU32(&buf[offs+4])+8+1) & ~1U; // round to nearest 2
  }
  return 0;
}
uint8_t *riffGetIndex(uint8_t *buf, int idx, const char *fourcc) {
  assert(is4CC(buf, "LIST")||is4CC(buf, "RIFF"));
  uint32_t listEnd = getU32(&buf[4])+8;
  uint32_t offs=12;
  while (offs<listEnd) {
    if (idx <= 0) {
      if (!is4CC(&buf[offs], fourcc)) {
        jsExceptionHere(JSET_ERROR, "RIFF LIST idx %d %c%c%c%c not correct\n", idx, buf[offs],buf[offs+1],buf[offs+2],buf[offs+3]);
        return 0;
      }
      return &buf[offs+8];
    }
    idx--;
    offs += (getU32(&buf[offs+4])+8+1) & ~1U; // round to nearest 2
  }
  return 0;
}
void riffListShow(uint8_t *buf, int pad) {
  assert(is4CC(buf, "LIST")||is4CC(buf, "RIFF"));
  for (int i=0;i<pad;i++) jsiConsolePrintf("  ");
  uint32_t listEnd = getU32(&buf[4])+8;
  jsiConsolePrintf("LIST %c%c%c%c (%db)\n", buf[8],buf[9],buf[10],buf[11],listEnd);
  uint32_t offs=12;
  while (offs<listEnd) {
    for (int i=0;i<pad;i++) jsiConsolePrintf("  ");
    if (is4CC(&buf[offs], "LIST")) {
      riffListShow(&buf[offs], pad+1);
    } else
      jsiConsolePrintf("- %c%c%c%c\n", buf[offs+0],buf[offs+1],buf[offs+2],buf[offs+3]);
    offs += (getU32(&buf[offs+4])+8+1) & ~1U; // round to nearest 2
  }
}

bool aviLoad(uint8_t *buf, int len, AviInfo *result, bool debugInfo) {
  if (!is4CC(buf,"RIFF")) {
    jsExceptionHere(JSET_ERROR, "Not RIFF %c%c%c%c\n", buf[0],buf[1],buf[2],buf[3]);
    return 0;
  }
  /*riffListShow(buf,0);

  AVI read 40960 40960 RIFF
LIST AVI  (1777854b)
  LIST hdrl (9938b)
  - avih
      LIST strl (5352b)
    - strh
    - strf
    - JUNK
    - vprp
      LIST strl (4242b)
    - strh
    - strf
    - JUNK
  - JUNK
  LIST INFO (34b)
  - ISFT
- JUNK
  LIST movi (1757446b)
  */

  if (!is4CC(&buf[8],"AVI ")) {
    jsExceptionHere(JSET_ERROR, "Not AVI file\n");
    return 0;
  }
  uint8_t *riffList = buf + 12; // skip RIFF+length+AVI tag
  if (!is4CC(riffList,"LIST") || !is4CC(&riffList[8],"hdrl")) {
    jsExceptionHere(JSET_ERROR, "Not LIST hdrl\n");
    return 0;
  }
  uint8_t *listPtr = riffList;
  uint32_t listEnd = (listPtr-buf) + getU32(&listPtr[4])+8;
  if (debugInfo) riffListShow(listPtr,0);
  MainAVIHeader *aviHeader = (MainAVIHeader*)riffGetIndex(listPtr, 0, "avih");
  if (!aviHeader) {
    jsExceptionHere(JSET_ERROR, "No header found\n");
    return 0;
  }
  if (debugInfo) {
    jsiConsolePrintf("AVI w=%d h=%d fps=%d\n",aviHeader->dwWidth, aviHeader->dwHeight, 1000000/aviHeader->dwMicroSecPerFrame);
  }
  result->width = aviHeader->dwWidth;
  result->height = aviHeader->dwHeight;
  result->usPerFrame = aviHeader->dwMicroSecPerFrame;
  result->audioBufferSize = 0;
  result->audioSampleRate = 0;
  for (int stream=0;stream<aviHeader->dwStreams;stream++) {
    uint8_t *streamListPtr = riffGetIndex(listPtr, 1+stream, "LIST")-8;
    if (!streamListPtr) {
      jsExceptionHere(JSET_ERROR, "No stream list %d\n");
      return 0;
    }
    AVIStreamHeader *streamHeader = (AVIStreamHeader*)riffGetIndex(streamListPtr, 0, "strh");
    if (!streamHeader) {
      jsExceptionHere(JSET_ERROR, "No stream %d\n");
      return 0;
    }
    char cc[10];
    memcpy(cc,&streamHeader->fccType,4);
    cc[4]=' ';
    memcpy(&cc[5],&streamHeader->fccHandler,4);
    cc[9]=0;
    if (debugInfo) {
      jsiConsolePrintf("Stream %d %s %d\n", stream, cc, streamHeader->dwStart);
    }
    if (is4CC(&streamHeader->fccType,"vids")) {
      BITMAPINFOHEADER *bmpHeader = (BITMAPINFOHEADER*)riffGetIndex(streamListPtr, 1, "strf");
      if (!bmpHeader) {
        jsExceptionHere(JSET_ERROR, "No BMPHEADER %d\n");
        return 0;
      }
      if (debugInfo) {
        jsiConsolePrintf("  - w=%d h=%d bpp=%d\n", bmpHeader->biWidth, bmpHeader->biHeight, bmpHeader->biBitCount);
      }
      // we're just assuming it's 8 bit, with palette
      uint8_t *palette = (uint8_t*)bmpHeader + sizeof(BITMAPINFOHEADER);
      for (int i=0;i<256;i++) { // RGBA format
        int ri = palette[0], gi=palette[1], bi=palette[2];
        result->palette[i] = (uint16_t)((ri>>3) | (gi>>2)<<5 | (bi>>3)<<11);
        palette+=4;
      }
    } else if (is4CC(&streamHeader->fccType,"auds")) {
      WAVHEADER *wavHeader = (WAVHEADER*)riffGetIndex(streamListPtr, 1, "strf");
       if (!wavHeader) {
        jsExceptionHere(JSET_ERROR, "No WAVHEADER %d\n");
        return 0;
      }
      if (wavHeader->formatTag!=1 || wavHeader->channels!=1 || wavHeader->size!=16) {
        jsExceptionHere(JSET_ERROR, "Not Mono 16 bit WAV (fmt=%d, channels=%d, size=%d)\n", wavHeader->formatTag, wavHeader->channels, wavHeader->size);
        return 0;
      }
      result->audioSampleRate = wavHeader->sampleRate;
      result->audioBufferSize = streamHeader->dwSuggestedBufferSize;
      if (debugInfo) {
        jsiConsolePrintf("  - %dhz mono 16b (%db buffer size)\n",  result->audioSampleRate, result->audioBufferSize);
      }
    }
  }
  uint8_t *moviPtr = riffListFind(buf, "movi");
  if (!moviPtr) {
    jsExceptionHere(JSET_ERROR, "No 'movi' found\n");
    return 0;
  }
  // FIXME set result->audioBufferSize to size of first audio record
  result->streamOffset = moviPtr+4-buf;
//  jsiConsolePrintf("Video offset %d \n", result->videoOffset);
  return true;
}

bool wavLoad(uint8_t *buf, int len, AviInfo *result, bool debugInfo) {
  result->width = 0;
  result->height = 0;
  result->usPerFrame = 0;
  result->audioBufferSize = 0;
  result->audioSampleRate = 0;

  if (!is4CC(buf,"RIFF")) {
    jsExceptionHere(JSET_ERROR, "Not RIFF %c%c%c%c\n", buf[0],buf[1],buf[2],buf[3]);
    return 0;
  }
  //riffListShow(buf,0);
/*
LIST WAVE (61470b)
- fmt
  LIST INFO (34b)
  - ISFT
- data
  */

  if (!is4CC(&buf[8],"WAVE ")) {
    jsExceptionHere(JSET_ERROR, "Not WAV file\n");
    return 0;
  }
  uint8_t *riffFmt = buf + 12; // skip RIFF+length+WAVE tag
  if (!is4CC(riffFmt,"fmt ")) {
    jsExceptionHere(JSET_ERROR, "Expecting 'fmt' tag\n");
    return 0;
  }
  uint8_t *listPtr = riffFmt;
  WAVHEADER *wavHeader = (WAVHEADER*)(riffFmt+8);
  if (wavHeader->formatTag!=1 || wavHeader->channels!=1 || wavHeader->size!=16) {
    jsExceptionHere(JSET_ERROR, "Not Mono 16 bit WAV (fmt=%d, channels=%d, size=%d)\n", wavHeader->formatTag, wavHeader->channels, wavHeader->size);
    return 0;
  }
  result->audioSampleRate = wavHeader->sampleRate;
  result->audioBufferSize = 0;

  uint8_t *dataStartPtr = riffListFind(buf, "data");
  if (!dataStartPtr) {
    jsExceptionHere(JSET_ERROR, "Data not found\n");
    return 0;
  }
  //jsiConsolePrintf("Data 0x%08x\n", dataStartPtr-buf);

  result->streamOffset = dataStartPtr-buf;
  return true;
}