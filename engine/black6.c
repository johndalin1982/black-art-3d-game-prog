#include <io.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <bios.h>
#include <fcntl.h>
#include <memory.h>
#include <malloc.h>
#include <math.h>
#include <string.h>

#include "black3.h"
#include "black6.h"

int soundLoad(char* filename, SoundPtr sound, int translate) {
    // this function will load a sound from disk into memory and pre-format
    // it in preparation to be played
    unsigned char FAR* tempPtr;     // temporary pointer used to load sound
    unsigned char FAR* soundPtr;    // pointer to sound data
    unsigned int bytesRead,         // used to track number of bytes read by DOS
                 sizeOfFile,        // the total size of the VOC file in bytes
                 headerLength;      // the length of the header portion of VOC file
    int soundHandle;                // DOS file handle

#ifndef DOS_32_BIT
    unsigned int segment,           // segment of sound data memory
                 paragraphs;        // number of 16 byte paragraphs sound takes up
#endif

    // open the sound file, use DOS file and memory allocation to make sure
    // memory is on a 16 byte or paragraph boundary
    if (_dos_open(filename, _O_RDONLY, &soundHandle) != 0) {
        printf("Sound System - Couldn't open %s\n", filename);
        return 0;
    }

    // compute number of paragraphs that sound file needs
    sizeOfFile = _filelength(soundHandle);

#ifdef DOS_32_BIT
    // DOS4GW: Use flat memory allocation (malloc puts it in first 1MB automatically)
    soundPtr = (unsigned char*)MALLOC(sizeOfFile);
    if (!soundPtr) {
        printf("Sound System - Couldn't allocate memory\n");
        _dos_close(soundHandle);
        return 0;
    }
#else
    // 16-bit mode: Use DOS paragraph allocation
    paragraphs = 1 + sizeOfFile / 16;

    // allocate the memory on a paragraph boundary
    _dos_allocmem(paragraphs, &segment);

    // point data pointer to allocated data area
    //_FP_SEG(soundPtr) = segment;
    //_FP_OFF(soundPtr) = 0;
    soundPtr = (unsigned char FAR*)(((unsigned long)segment) << 16);
#endif

    // alias pointer to memory storage area
    tempPtr = soundPtr;

    // read in blocks of 16k until file is loaded
    do {
        // load next block
        _dos_read(soundHandle, tempPtr, 0x4000, &bytesRead);

        // adjust pointer
        tempPtr += bytesRead;
    } while (bytesRead == 0x4000);

    // close the file
    _dos_close(soundHandle);

    // make sure it's a voc file, test for "Creative"
    if (soundPtr[0] != 'C' || soundPtr[1] != 'r') {
        printf("%s is not a VOC file!", filename);

        // de-allocate the memory
#ifdef DOS_32_BIT
        FREE(soundPtr);
#else
        _dos_freemem(_FP_SEG(soundPtr));
#endif

        // return failure
        return 0;
    }

    // compute start of sound data
    headerLength = (unsigned int)soundPtr[20];

    // point buffer pointer to start of VOC file in memory
    sound->buffer = soundPtr;

    // set up the SndStruc for DIGPAK
    sound->ss.sound = (unsigned char FAR*)(soundPtr + headerLength + 4);
    sound->ss.sndLen = (unsigned short)(sizeOfFile - headerLength);
    sound->ss.isPlaying = (short FAR*)&sound->status;
    sound->ss.frequency = (short)(-1000000L / ((int)soundPtr[headerLength + 4] - 256));

    // now format data for sound card if requested
    if (translate) {
        soundTranslate(sound);
    }

    // return success
    return 1;
}

void soundTranslate(SoundPtr sound) {
    // this function calls the DIGPAK function massage audio to translate
    // the raw audio data into the proper format for the sound card that
    // the sound system is running on.
    unsigned char FAR* buffer = (unsigned char FAR*)&sound->ss;

#ifdef DOS_32_BIT
    _asm {
        mov ax, 068Ah           ; function 3: MassageAudio
        mov esi, buffer         ; load flat pointer into ESI
        int 66h                 ; call DIGPAK
    }
#else
    _asm {
        push ds         ; save DS and SI on stack
        push si
        mov ax,068Ah    ; function 3: MassageAudio
        lds si,buffer   ; move address of sound in DS:SI
        int 66h         ; call DIGPAK
        pop si          ; restore DS and SI from stack
        pop ds
    }
#endif
}

void soundUnload(SoundPtr sound) {
    // this function deletes the sound from memory
#ifdef DOS_32_BIT
    FREE(sound->buffer);
#else
    _dos_freemem(_FP_SEG(sound->buffer));
#endif

    sound->buffer = NULL;
}

void soundPlay(SoundPtr sound) {
    // this function plays the sound pointed to by the sound structure
    unsigned char FAR* buffer = (unsigned char FAR*)&sound->ss;

#ifdef DOS_32_BIT
    _asm {
        mov ax, 068Bh           ; function 4: DigPlay2
        mov esi, buffer         ; load flat pointer into ESI
        int 66h                 ; call DIGPAK
    }
#else
    _asm {
        push ds         ; save DS and SI on stack
        push si
        mov ax,068Bh    ; function 4: DigPlay2
        lds si,buffer   ; move address of sound in DS:SI
        int 66h         ; call DIGPAK
        pop si          ; restore DS and SI from stack
        pop ds
    }
#endif
}

int soundStatus(void) {
    // this function will return the status of DIGPAK i.e. is a sound playing or not

    _asm {
        mov ax,0689h    ; function 2: SoundStatus
        int 66h         ; call DIGPAK
    }

    // on exit AX will be used as the return value, if 1 then a sound is playing
    // 0 if a sound is not playing
}

void soundStop(void) {
    // this function will stop a currently playing sound
    _asm {
        mov ax,068Fh    ; function 8: StopSound
        int 66h         ; call DIGPAK
    }
}

int musicLoad(char* filename, MusicPtr music) {
    // this function will load a xmidi file from disk into memory and register it
    unsigned char FAR* tempPtr;     // temporary pointer used to load music
    unsigned char FAR* xmidiPtr;    // pointer to xmidi data
    unsigned int bytesRead;         // used to track number of bytes read by DOS
    long sizeOfFile;                // the total size of the xmidi file in bytes
    int xmidiHandle;                // DOS file handle

#ifndef DOS_32_BIT
    unsigned int segment,           // segment of music data memory
                 paragraphs;        // number of 16 byte paragraphs music takes up
#endif

    // open the extended xmidi file, use DOS file and memory allocation to make sure
    // memory is on a 16 byte or paragraph boundary
    if (_dos_open(filename, _O_RDONLY, &xmidiHandle) != 0) {
        printf("Music System - Couldn't open %s\n", filename);
        return 0;
    }

    // compute number of paragraphs that sound file needs
    sizeOfFile = _filelength(xmidiHandle);

#ifdef DOS_32_BIT
    // DOS4GW: Use flat memory allocation
    xmidiPtr = (unsigned char*)MALLOC(sizeOfFile);
    if (!xmidiPtr) {
        printf("Music System - Couldn't allocate memory\n");
        _dos_close(xmidiHandle);
        return 0;
    }
#else
    // 16-bit mode: Use DOS paragraph allocation
    paragraphs = 1 + sizeOfFile / 16;

    // allocate the memory on a paragraph boundary
    _dos_allocmem(paragraphs, &segment);

    // point data pointer to allocated data area
    //_FP_SEG(xmidiPtr) = segment;
    //_FP_OFF(xmidiPtr) = 0;
    xmidiPtr = (unsigned char FAR*)(((unsigned long)segment) << 16);
#endif

    // alias pointer to memory storage area
    tempPtr = xmidiPtr;

    // read in blocks of 16k until file is loaded
    do {
        // load next block
        _dos_read(xmidiHandle, tempPtr, 0x4000, &bytesRead);

        // adjust pointer
        tempPtr += bytesRead;
    } while (bytesRead == 0x4000);

    // close the file
    _dos_close(xmidiHandle);

    // set up the music structure
    music->buffer = xmidiPtr;
    music->size = sizeOfFile;
    music->status = 0;

    // now register the xmidi file with MIDPAK
    if ((music->registerInfo = musicRegister(music)) == XMIDI_UNREGISTERED) {
        // delete the memory
        musicUnload(music);

        // return an error
        return 0;
    }

    // else return success
    return 1;
}

int musicRegister(MusicPtr music) {
    // this function registers the xmidi music with MIDPAK, so that it can be played
#ifdef DOS_32_BIT
    unsigned long linearAddr;   // 32-bit linear address
    unsigned short lengthLow, lengthHi;

    // In DOS4GW, just use the flat pointer as-is
    linearAddr = (unsigned long)music->buffer;
    lengthLow = (unsigned short)(music->size & 0xFFFF);
    lengthHi = (unsigned short)(music->size >> 16);

    _asm {
        push esi
        push edi
        mov ax, 704h            ; function #5: RegisterXmidi
        mov ebx, linearAddr     ; EBX = full 32-bit linear address
        xor ecx, ecx            ; ECX = 0 (selector not used in flat model)
        mov si, lengthLow       ; SI = low word of length
        mov di, lengthHi        ; DI = high word of length
        int 66h                 ; call MIDPAK
        movzx eax, ax           ; zero-extend result
        pop edi
        pop esi
    }

    // return value will be in EAX
#else
    unsigned int xmidOff,   // offset of midi file
                 xmidSeg,       // segment of midi file
                 lengthLow,     // length of midi file in bytes
                 lengthHi;

    // extract segment and offset of music buffer
    xmidOff = _FP_OFF(music->buffer);
    xmidSeg = _FP_SEG(music->buffer);

    // extract the low word and high word of xmidi file length
    lengthLow = music->size;
    lengthHi = music->size >> 16;

    // call MIDPAK
    _asm {
        push si             ; save si and di
        push di
        mov ax,704h         ; function #5: RegisterXmidi
        mov bx,xmidOff      ; offset of xmidi data
        mov cx,xmidSeg      ; segment of xmidi data
        mov si,lengthLow    ; low word of xmidi length
        mov di,lengthHi     ; hi word of xmidi length
        int 66h             ; call MIDPAK
        pop di              ; restore si and di
        pop si
    }

    // return value will be in AX
#endif
}

void musicUnload(MusicPtr music) {
    // this function deletes the xmidi file data from memory
#ifdef DOS_32_BIT
    FREE(music->buffer);
#else
    _dos_freemem(_FP_SEG(music->buffer));
#endif
    music->buffer = NULL;
}

int musicPlay(MusicPtr music, int sequence) {
    // this function plays an xmidi file from memory
#ifdef DOS_32_BIT
    _asm {
        mov ax, 702h
        mov ebx, sequence
        int 66h
        movsx eax, ax       ; sign-extend AX to EAX if return values can be negative
    }
#else
    _asm {
        mov ax,702h     ; function #3: PlaySequence
        mov bx,sequence ; which sequence to play 0,1,2....
        int 66h         ; call MIDPAK
    }
#endif

    // return value is in AX/EAX, 1 success, 0 sequence not available
}

void musicStop(void) {
    // this function will stop the song currently playing
    _asm {
        mov ax,705h     ; function #6: MidiStop
        int 66h         ; call MIDPAK
    }
}

void musicResume(void) {
    // this function resumes a previously stopped xmidi sequence
    _asm {
        mov ax,70Bh     ; function #12: ResumePlaying
        int 66h         ; call MIDPAK
    }
}

int musicStatus(void) {
    // this function returns the status of a playing sequence
    _asm {
        mov ax,70Ch     ; function #13: SequenceStatus
        int 66h         ; call MIDPAK
    }

    // return value is in AX/EAX
}
