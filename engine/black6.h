#include "black3.h"

// return values for digital sound status function
#define SOUND_STOPPED   0   // no sound is playing
#define SOUND_PLAYING   1   // a sound is playing

// return values for the midi sequence status function
#define SEQUENCE_STOPPED        0   // the current sequence is stopped
#define SEQUENCE_PLAYING        1   // the current sequence is playing
#define SEQUENCE_COMPLETE       2   // the current sequence has completed
#define SEQUENCE_UNAVAILABLE    0   // this sequence is unavailable

// these return values are used to determine what happened when a midi file
// has been registered
#define XMIDI_UNREGISTERED  0   // the midi file couldn't be registered at all
#define XMIDI_BUFFERED      1   // the midi file was registered and buffered
#define XMIDI_UNBUFFERED    2   // the midi file was registered, but was too
                                // big to be buffered, hence, the caller
                                // needs to keep the midi data resident in memory

// the DIGPAK sound structure — DIGPAK is a real-mode TSR and reads this with
// an unpacked 12-byte layout (4-byte pointer, 2-byte length, 4-byte pointer,
// 2-byte frequency). Default 32-bit Watcom alignment would insert 2 bytes of
// padding between sndLen and isPlaying, shifting frequency to offset 12 and
// making DIGPAK read garbage as the playback rate — pack to 1 to lock the
// layout for both builds.
#pragma pack(push, 1)
typedef struct SndStrucType {
    unsigned char FAR* sound;   // a pointer to the raw sound data
    unsigned short sndLen;      // the length of the sound data in bytes
    short FAR* isPlaying;       // a pointer to a variable that will be
                                // used to hold the status of a playing sound
    short frequency;            // the frequency in hertz that the
                                // sound should be played at
} SndStruc, *SndStrucPtr;
#pragma pack(pop)

// our high level sound structure
typedef struct SoundType {
    unsigned char FAR* buffer;  // pointer to the start of VOC file
    short status;               // the current status of the sound
    SndStruc ss;                // the DIGPAK sound structure
#ifdef DOS_32_BIT
    unsigned short dosSegment;  // real-mode segment of the DOS block holding
                                // [VOC data | SndStruc | status word]
    unsigned short dosSelector; // PM selector for the same block (used by dpmiFreeDos)
    unsigned short ssOffset;    // offset of the SndStruc inside the DOS block
#endif
} Sound, *SoundPtr;

// this holds a midi file
typedef struct MusicType {
    unsigned char FAR* buffer;  // pointer to midi data
    long size;                  // size of midi file in bytes
    int status;                 // status of long
    int registerInfo;           // return values of RegisterXmidiFile
#ifdef DOS_32_BIT
    unsigned short dosSegment;  // real-mode segment of XMIDI data in DOS memory
    unsigned short dosSelector; // PM selector for the same block
#endif
} Music, *MusicPtr;

int soundLoad(char* filename, SoundPtr sound, int translate);
void soundTranslate(SoundPtr sound);
void soundUnload(SoundPtr sound);
void soundPlay(SoundPtr sound);
int soundStatus(void);
void soundStop(void);
int musicLoad(char* filename, MusicPtr music);
int musicRegister(MusicPtr music);
void musicUnload(MusicPtr music);
int musicPlay(MusicPtr music, int sequence);
void musicStop(void);
void musicResume(void);
int musicStatus(void);
