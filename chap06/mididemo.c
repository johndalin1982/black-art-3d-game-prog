// mididemo.c - MIDI Music Demo

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

// include the sound library
#include "black3.h"
#include "black6.h"

void main(int argc, char** argv) {
    char filename[16];  // the .XMI extended midi filename
    int done = 0,   // main loop exit flag
        loaded = 0, // tracks if a file has been loaded
        sequence,   // sequence of XMI extended midi file to be played
        select;     // the user's input
    Music song;     // the music structure

    // main event loop
    while (!done) {
        // print out menu

        printf("Extended MIDI Menu\n");
        printf("1. Load Extended MIDI file.\n");
        printf("2. Play Sequence.\n");
        printf("3. Stop Sequence.\n");
        printf("4. Resume Sequence.\n");
        printf("5. Unload Extended MIDI File from Memory.\n");
        printf("6. Print Status.\n");
        printf("7. Exit Program.\n\n");

        printf("Select One ?");

        // get input
        scanf("%d", &select);

        // what does user want to do?
        switch (select) {
            case 1: // Load Extended MIDI file
            {
                printf("Enter Filename of song ?");
                scanf("%s", filename);

                // if a song is already loaded then delete it
                if (loaded) {
                    // stop music and unload sound
                    musicStop();
                    musicUnload(&song);
                }

                // load the new file
                if (musicLoad(filename, &song)) {
                    printf("Music file successfully loaded...\n");

                    // flag that a file is loaded
                    loaded = 1;
                } else {
                    // error
                    printf("Sorry, the file %s couldn't be loaded!\n", filename);
                }
            } break;

            case 2: // Play Sequence
            {
                // make sure a midi file had been loaded
                if (loaded) {
                    printf("Which Sequence 0..n ?");
                    scanf("%d", &sequence);

                    // play the requested sequence
                    musicPlay(&song, sequence);
                } else {
                    printf("You must first load an extended MIDI file.\n");
                }
            } break;

            case 3: // Stop Sequence
            {
                // make sure a midi file has been loaded
                if (loaded) {
                    musicStop();
                } else {
                    printf("You must first load an extended MIDI file.\n");
                }
            } break;

            case 4: // Resume Sequence
            {
                // make sure a midi file has been loaded
                if (loaded) {
                    musicResume();
                } else {
                    printf("You must first load an extended MIDI file.\n");
                }
            } break;

            case 5: // Unload Extended MIDI File from Memory
            {
                // make sure a midi file has been loaded
                if (loaded) {
                    musicStop();
                    musicUnload(&song);
                    loaded = 0;
                } else {
                    printf("You must first load an extended MIDI file.\n");
                }
            } break;

            case 6: // Print Status
            {
                printf("MIDPAK Status = %d\n", musicStatus());
            } break;

            case 7: // Exit Program
            {
                // delete music and stop
                if (loaded) {
                    musicStop();
                    musicUnload(&song);
                    loaded = 0;
                }

                done = 1;
            } break;

            default:
            {
                printf("Invalid Selection!\n");
            }
        }
    }

    // unload file if there is one
    if (loaded) {
        musicUnload(&song);
    }
}
