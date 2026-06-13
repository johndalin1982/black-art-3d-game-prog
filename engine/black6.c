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

#include "black6.h"

#ifdef DOS_32_BIT
#include "dpmi.h"
#endif

#ifndef DOS_32_BIT
int soundLoad(char* filename, SoundPtr sound, int translate) {
    // this function will load a sound from disk into memory and pre-format
    // it in preparation to be played
    unsigned char far* tempPtr;     // temporary pointer used to load sound
    unsigned char far* soundPtr;    // pointer to sound data
    unsigned int bytesRead,         // used to track number of bytes read by DOS
                 sizeOfFile,        // the total size of the VOC file in bytes
                 headerLength,      // the length of the header portion of VOC file
                 segment,           // segment of sound data memory
                 paragraphs;        // number of 16 byte paragraphs sound takes up
    int soundHandle;                // DOS file handle

    // open the sound file, use DOS file and memory allocation to make sure
    // memory is on a 16 byte or paragraph boundary
    if (_dos_open(filename, _O_RDONLY, &soundHandle) != 0) {
        printf("Sound System - Couldn't open %s\n", filename);
        return 0;
    }

    // compute number of paragraphs that sound file needs
    sizeOfFile = _filelength(soundHandle);
    paragraphs = 1 + sizeOfFile / 16;

    // allocate the memory on a paragraph boundary
    _dos_allocmem(paragraphs, &segment);

    // point data pointer to allocated data area
    soundPtr = (unsigned char far*)MK_FP(segment, 0);

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
        _dos_freemem(_FP_SEG(soundPtr));

        // return failure
        return 0;
    }

    // compute start of sound data
    headerLength = (unsigned int)soundPtr[20];

    // point buffer pointer to start of VOC file in memory
    sound->buffer = soundPtr;

    // set up the SndStruc for DIGPAK
    sound->ss.sound = (unsigned char far*)(soundPtr + headerLength + 4);
    sound->ss.sndLen = (unsigned short)(sizeOfFile - headerLength);
    sound->ss.isPlaying = (short far*)&sound->status;
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
    unsigned char far* buffer = (unsigned char far*)&sound->ss;

    _asm {
        push ds         ; save DS and SI on stack
        push si
        mov ax,068Ah    ; function 3: MassageAudio
        lds si,buffer   ; move address of sound in DS:SI
        int 66h         ; call DIGPAK
        pop si          ; restore DS and SI from stack
        pop ds
    }
}

void soundUnload(SoundPtr sound) {
    // this function deletes the sound from memory
    _dos_freemem(_FP_SEG(sound->buffer));

    sound->buffer = NULL;
}

void soundPlay(SoundPtr sound) {
    // this function plays the sound pointed to by the sound structure
    unsigned char far* buffer = (unsigned char far*)&sound->ss;

    _asm {
        push ds         ; save DS and SI on stack
        push si
        mov ax,068Bh    ; function 4: DigPlay2
        lds si,buffer   ; move address of sound in DS:SI
        int 66h         ; call DIGPAK
        pop si          ; restore DS and SI from stack
        pop ds
    }
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
    unsigned char far* tempPtr;     // temporary pointer used to load music
    unsigned char far* xmidiPtr;    // pointer to xmidi data
    unsigned int bytesRead,         // used to track number of bytes read by DOS
                 segment,           // segment of music data memory
                 paragraphs;        // number of 16 byte paragraphs music takes up
    long sizeOfFile;                // the total size of the xmidi file in bytes
    int xmidiHandle;                // DOS file handle

    // open the extended xmidi file, use DOS file and memory allocation to make sure
    // memory is on a 16 byte or paragraph boundary
    if (_dos_open(filename, _O_RDONLY, &xmidiHandle) != 0) {
        printf("Music System - Couldn't open %s\n", filename);
        return 0;
    }

    // compute number of paragraphs that sound file needs
    sizeOfFile = _filelength(xmidiHandle);
    paragraphs = 1 + sizeOfFile / 16;

    // allocate the memory on a paragraph boundary
    _dos_allocmem(paragraphs, &segment);

    // point data pointer to allocated data area
    xmidiPtr = (unsigned char far*)MK_FP(segment, 0);

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
}

void musicUnload(MusicPtr music) {
    // this function deletes the xmidi file data from memory
    _dos_freemem(_FP_SEG(music->buffer));
    music->buffer = NULL;
}

int musicPlay(MusicPtr music, int sequence) {
    // this function plays an xmidi file from memory
    _asm {
        mov ax,702h     ; function #3: PlaySequence
        mov bx,sequence ; which sequence to play 0,1,2....
        int 66h         ; call MIDPAK
    }

    // return value is in AX, 1 success, 0 sequence not available
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

    // return value is in AX
}
#else

// 32-bit DOS/4GW build: bridge to real-mode DIGPAK/MIDPAK TSRs via DPMI INT 31h.
// All buffers DIGPAK/MIDPAK dereference (VOC data, the SndStruc itself, XMIDI data,
// status word) must live in DOS conventional memory below 1 MB.

// DIGPAK is real-mode code that reads SndStruc with a fixed 12-byte layout.
// Catch any future regression where padding sneaks back in (default Watcom -zp8
// would insert 2 padding bytes before isPlaying and shift frequency to offset 12).
typedef char SndStrucPackedCheck[sizeof(SndStruc) == 12 ? 1 : -1];

#define AUDIO_INT             0x66

#define DIGPAK_SOUND_STATUS   0x0689   // function 2
#define DIGPAK_DIG_PLAY2      0x068B   // function 4
#define DIGPAK_MASSAGE_AUDIO  0x068A   // function 3
#define DIGPAK_STOP_SOUND     0x068F   // function 8

#define MIDPAK_PLAY_SEQ       0x0702   // function 3
#define MIDPAK_REGISTER_XMI   0x0704   // function 5
#define MIDPAK_MIDI_STOP      0x0705   // function 6
#define MIDPAK_RESUME         0x070B   // function 12
#define MIDPAK_SEQ_STATUS     0x070C   // function 13

static int AudioProbed = 0;
static int AudioPresent = 0;

static int audioPresent(void) {
    // Replicates DIGPAK's own CheckIn procedure — read INT 66h vector, look
    // 6 bytes BEFORE the handler entry, verify the ASCII signature "KERN"
    // (DIGPAK) or "MIDI" (MIDPAK). Defends against an unrelated TSR having
    // hooked INT 66h.
    // Source: https://github.com/jratcliff63367/digpak/blob/master/DIGPLAY.ASM
    //         (proc CheckIn near, near line 418)
    unsigned short  seg;
    unsigned short  off;
    unsigned char*  sig;

    if (AudioProbed) {
        return AudioPresent;
    }
    AudioProbed = 1;

    if (!dpmiGetVector(AUDIO_INT, &seg, &off)) {
        AudioPresent = 0;
        return 0;
    }

    if (seg == 0 && off == 0) {
        AudioPresent = 0;
        return 0;
    }

    // Real-mode (seg:off) - 6 in DOS/4GW flat space.
    sig = (unsigned char*)(((unsigned int)seg << 4) + off - 6);

    // DIGPAK signature: 'K','E','R','N' (4Bh 45h 52h 4Eh)
    if (sig[0] == 'K' && sig[1] == 'E' && sig[2] == 'R' && sig[3] == 'N') {
        AudioPresent = 1;
        return 1;
    }

    // MIDPAK signature: 'M','I','D','I' (4Dh 49h 44h 49h). MIDPAK typically
    // sits on top of DIGPAK and forwards DIGPAK functions down.
    if (sig[0] == 'M' && sig[1] == 'I' && sig[2] == 'D' && sig[3] == 'I') {
        AudioPresent = 1;
        return 1;
    }

    AudioPresent = 0;
    return 0;
}

static unsigned char* dosFlatPtr(unsigned short segment, unsigned short offset) {
    // DOS/4GW identity-maps the first MB of physical memory into the flat data
    // selector, so the linear address of a real-mode (seg:off) is (seg << 4 + off).
    return (unsigned char*)(((unsigned int)segment << 4) + offset);
}

int soundLoad(char* filename, SoundPtr sound, int translate) {
    int             handle;
    unsigned int    bytesRead;
    unsigned int    sizeOfFile;
    unsigned int    headerLength;
    unsigned int    ssOffset;
    unsigned int    statusOffset;
    unsigned int    totalParagraphs;
    unsigned char*  dosFlat;
    unsigned char*  readPtr;
    unsigned short  dosSegment;
    unsigned short  dosSelector;
    SndStruc*       dosSs;

    sound->buffer      = NULL;
    sound->dosSegment  = 0;
    sound->dosSelector = 0;

    if (!audioPresent()) {
        return 0;
    }

    if (_dos_open(filename, _O_RDONLY, &handle) != 0) {
        printf("Sound System - Couldn't open %s\n", filename);
        return 0;
    }

    sizeOfFile   = _filelength(handle);

    // DOS block layout:
    //   [0 .. sizeOfFile)              VOC file data
    //   [ssOffset .. + sizeof(SndStruc))   the SndStruc DIGPAK reads via DS:SI
    //   [statusOffset .. + 2)          status word DIGPAK writes via SndStruc.isPlaying
    ssOffset        = (sizeOfFile + 1) & ~1U;
    statusOffset    = ssOffset + sizeof(SndStruc);
    totalParagraphs = 1 + (statusOffset + 2) / 16;

    if (!dpmiAllocDos(totalParagraphs, &dosSegment, &dosSelector)) {
        printf("Sound System - DPMI alloc failed for %s\n", filename);
        _dos_close(handle);
        return 0;
    }

    dosFlat = dosFlatPtr(dosSegment, 0);

    // read VOC straight into DOS conventional memory
    readPtr = dosFlat;
    do {
        _dos_read(handle, readPtr, 0x4000, &bytesRead);
        readPtr += bytesRead;
    } while (bytesRead == 0x4000);

    _dos_close(handle);

    if (dosFlat[0] != 'C' || dosFlat[1] != 'r') {
        printf("%s is not a VOC file!", filename);
        dpmiFreeDos(dosSelector);
        return 0;
    }

    headerLength = (unsigned int)dosFlat[20];

    // Build the SndStruc inside the DOS block. The FAR pointers stored in it are
    // real-mode seg:off pairs (high 16 = segment, low 16 = offset) — DIGPAK reads
    // them with real-mode segment loads. The FAR* type still has 4-byte storage
    // in 32-bit Watcom (FAR is empty), so we cast the seg:off bit pattern in.
    dosSs            = (SndStruc*)(dosFlat + ssOffset);
    dosSs->sound     = (unsigned char FAR*)(((unsigned int)dosSegment << 16)
                                            | ((headerLength + 4) & 0xFFFF));
    dosSs->sndLen    = (unsigned short)(sizeOfFile - headerLength);
    dosSs->isPlaying = (short FAR*)(((unsigned int)dosSegment << 16)
                                    | (statusOffset & 0xFFFF));
    dosSs->frequency = (short)(-1000000L / ((int)dosFlat[headerLength + 4] - 256));

    // status word starts cleared
    *(short*)(dosFlat + statusOffset) = 0;

    // mirror to the PM Sound struct (so callers can read frequency etc.)
    sound->buffer      = dosFlat;
    sound->status      = 0;
    sound->ss          = *dosSs;
    sound->dosSegment  = dosSegment;
    sound->dosSelector = dosSelector;
    sound->ssOffset    = (unsigned short)ssOffset;

    if (translate) {
        soundTranslate(sound);
    }

    return 1;
}

void soundTranslate(SoundPtr sound) {
    DpmiRealModeRegs regs;

    if (!audioPresent() || sound->dosSegment == 0) {
        return;
    }

    memset(&regs, 0, sizeof(regs));
    regs.eax = DIGPAK_MASSAGE_AUDIO;
    regs.ds  = sound->dosSegment;
    regs.esi = sound->ssOffset;

    dpmiRealModeInt(AUDIO_INT, &regs);
}

void soundUnload(SoundPtr sound) {
    if (sound->dosSelector != 0) {
        dpmiFreeDos(sound->dosSelector);
    }

    sound->buffer      = NULL;
    sound->dosSegment  = 0;
    sound->dosSelector = 0;
}

void soundPlay(SoundPtr sound) {
    DpmiRealModeRegs regs;

    if (!audioPresent() || sound->dosSegment == 0) {
        return;
    }

    memset(&regs, 0, sizeof(regs));
    regs.eax = DIGPAK_DIG_PLAY2;
    regs.ds  = sound->dosSegment;
    regs.esi = sound->ssOffset;

    dpmiRealModeInt(AUDIO_INT, &regs);
}

int soundStatus(void) {
    DpmiRealModeRegs regs;

    if (!audioPresent()) {
        return SOUND_STOPPED;
    }

    memset(&regs, 0, sizeof(regs));
    regs.eax = DIGPAK_SOUND_STATUS;

    dpmiRealModeInt(AUDIO_INT, &regs);

    return (int)(regs.eax & 0xFFFF);
}

void soundStop(void) {
    DpmiRealModeRegs regs;

    if (!audioPresent()) {
        return;
    }

    memset(&regs, 0, sizeof(regs));
    regs.eax = DIGPAK_STOP_SOUND;

    dpmiRealModeInt(AUDIO_INT, &regs);
}

int musicLoad(char* filename, MusicPtr music) {
    int             handle;
    long            sizeOfFile;
    unsigned int    bytesRead;
    unsigned int    paragraphs;
    unsigned char*  dosFlat;
    unsigned char*  readPtr;
    unsigned short  dosSegment;
    unsigned short  dosSelector;

    music->buffer      = NULL;
    music->dosSegment  = 0;
    music->dosSelector = 0;

    if (!audioPresent()) {
        return 0;
    }

    if (_dos_open(filename, _O_RDONLY, &handle) != 0) {
        printf("Music System - Couldn't open %s\n", filename);
        return 0;
    }

    sizeOfFile = _filelength(handle);
    paragraphs = 1 + (unsigned int)(sizeOfFile / 16);

    if (!dpmiAllocDos(paragraphs, &dosSegment, &dosSelector)) {
        printf("Music System - DPMI alloc failed for %s\n", filename);
        _dos_close(handle);
        return 0;
    }

    dosFlat = dosFlatPtr(dosSegment, 0);

    readPtr = dosFlat;
    do {
        _dos_read(handle, readPtr, 0x4000, &bytesRead);
        readPtr += bytesRead;
    } while (bytesRead == 0x4000);

    _dos_close(handle);

    music->buffer      = dosFlat;
    music->size        = sizeOfFile;
    music->status      = 0;
    music->dosSegment  = dosSegment;
    music->dosSelector = dosSelector;

    if ((music->registerInfo = musicRegister(music)) == XMIDI_UNREGISTERED) {
        musicUnload(music);
        return 0;
    }

    return 1;
}

int musicRegister(MusicPtr music) {
    DpmiRealModeRegs regs;

    if (!audioPresent() || music->dosSegment == 0) {
        return XMIDI_UNREGISTERED;
    }

    memset(&regs, 0, sizeof(regs));
    regs.eax = MIDPAK_REGISTER_XMI;
    regs.ebx = 0;                                            // BX = XMIDI offset = 0
    regs.ecx = music->dosSegment;                            // CX = XMIDI segment
    regs.esi = (unsigned int)(music->size & 0xFFFF);         // SI = length low
    regs.edi = (unsigned int)((music->size >> 16) & 0xFFFF); // DI = length high

    dpmiRealModeInt(AUDIO_INT, &regs);

    return (int)(regs.eax & 0xFFFF);
}

void musicUnload(MusicPtr music) {
    if (music->dosSelector != 0) {
        dpmiFreeDos(music->dosSelector);
    }

    music->buffer      = NULL;
    music->dosSegment  = 0;
    music->dosSelector = 0;
}

int musicPlay(MusicPtr music, int sequence) {
    DpmiRealModeRegs regs;

    if (!audioPresent() || music->dosSegment == 0) {
        return 0;
    }

    memset(&regs, 0, sizeof(regs));
    regs.eax = MIDPAK_PLAY_SEQ;
    regs.ebx = (unsigned int)sequence;

    dpmiRealModeInt(AUDIO_INT, &regs);

    return (int)(regs.eax & 0xFFFF);
}

void musicStop(void) {
    DpmiRealModeRegs regs;

    if (!audioPresent()) {
        return;
    }

    memset(&regs, 0, sizeof(regs));
    regs.eax = MIDPAK_MIDI_STOP;

    dpmiRealModeInt(AUDIO_INT, &regs);
}

void musicResume(void) {
    DpmiRealModeRegs regs;

    if (!audioPresent()) {
        return;
    }

    memset(&regs, 0, sizeof(regs));
    regs.eax = MIDPAK_RESUME;

    dpmiRealModeInt(AUDIO_INT, &regs);
}

int musicStatus(void) {
    DpmiRealModeRegs regs;

    if (!audioPresent()) {
        return SEQUENCE_STOPPED;
    }

    memset(&regs, 0, sizeof(regs));
    regs.eax = MIDPAK_SEQ_STATUS;

    dpmiRealModeInt(AUDIO_INT, &regs);

    return (int)(regs.eax & 0xFFFF);
}

#endif
