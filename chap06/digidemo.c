// digidemo.c - Digital sound demo

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

// defines for the phrases
#define SOUND_WHAT      0
#define SOUND_WRONG     1
#define SOUND_CORRECT   2
#define SOUND_PLUS      3
#define SOUND_EQUAL     4

Sound Ones[11];     // this will hold the digital samples for 1..9
Sound Teens[11];    // this will hold the digital samples for 11-19
Sound Tens[11];     // this will hold the digital samples for 10,20,30,40...100
Sound Phrases[6];   // this will hold the phrases

void sayNumber(int number) {
    // this function uses digitized samples to construct whole numbers
    // note that the teens i.e. numbers from 11-19 have to be spoken as a
    // special case and can't be concatenated as the numbers 20-1 can!
    int onesPlace,
        tensPlace;

    // compute place values, use simple logic, more complex logic can be
    // derived that uses MODING and so forth, but better to see what's going
    // on. However, the main point of this is to see the digital library
    // in action, so focus on that aspect.

    // test for 0..9
    if (number < 10) {
        soundPlay(&Ones[number]);

        while (soundStatus() == SOUND_PLAYING);

        return;
    }

    // test for 11..19
    if (number >= 11 && number <= 19) {
        soundPlay(&Teens[number - 11]);

        while (soundStatus() == SOUND_PLAYING);

        return;
    }

    // now break number down into tens and ones
    tensPlace = number / 10;
    onesPlace = number % 10;

    // first say tens place
    soundPlay(&Tens[tensPlace - 1]);

    while (soundStatus() == SOUND_PLAYING);

    // now say ones place (if any)
    if (onesPlace) {
        soundPlay(&Ones[onesPlace]);

        while (soundStatus() == SOUND_PLAYING);
    }
}

void main(int argc, char** argv) {
    char filename[16];  // used to build up filename
    int number,
        number1,
        number2,
        answer,
        done = 0;   // exit flag
    float numProblems = 0,  // used to track performance of player
          numCorrect = 0;

    // load in the samples for the ones
    for (number = 1; number <= 9; number++) {
        // build the filename
        sprintf(filename, "N%d.VOC", number);
        printf("Loading file %s\n", filename);

        // load the sound
        soundLoad(filename, &Ones[number], 1);
    }

    // load in the samples for the teens
    for (number = 11; number <= 19; number++) {
        // build the filename
        sprintf(filename, "N%d.VOC", number);
        printf("Loading file %s\n", filename);

        // load the sound
        soundLoad(filename, &Teens[number - 11], 1);
    }

    // load in the samples for the tens
    for (number = 10; number <= 100; number += 10) {
        // build the filename
        sprintf(filename, "N%d.VOC", number);
        printf("Loading file %s\n", filename);

        // load the sound
        soundLoad(filename, &Tens[number / 10 - 1], 1);
    }

    // load the phrases
    printf("Loading the phrases...\n");
    soundLoad("what.voc", &Phrases[SOUND_WHAT], 1);
    soundLoad("wrong.voc", &Phrases[SOUND_WRONG], 1);
    soundLoad("correct.voc", &Phrases[SOUND_CORRECT], 1);
    soundLoad("plus.voc", &Phrases[SOUND_PLUS], 1);
    soundLoad("equal.voc", &Phrases[SOUND_EQUAL], 1);

    // main event loop, note this one is not real-time since it waits for user input!
    printf("         S P E A K   N   A D D  !!!\n\n\n");
    printf("This program will test your skills of addition while demonstrating\n");
    printf("the digital sound channel in action!\n\n");
    printf("Just answer each addition problem. To exit type in 0.\n\n\n");
    printf("Press any key to begin!!!\n\n");

    getch();

    srand(time(NULL));

    // the main event loop
    while (!done) {
        // select two random numbers to add, but make sure their sum is less than or equal to 100
        number1 = 1 + rand() % 99;
        number2 = 1 + rand() % (100 - number1);

        // ask user question
        printf("What is ");
        fflush(stdout);

        soundPlay(&Phrases[SOUND_WHAT]);

        while (soundStatus() == SOUND_PLAYING);

        printf("%d", number1);
        fflush(stdout);
        sayNumber(number1);
        printf(" + ");
        fflush(stdout);
        soundPlay(&Phrases[SOUND_PLUS]);

        while (soundStatus() == SOUND_PLAYING);

        timeDelay(15);
        printf("%d", number2);
        fflush(stdout);
        sayNumber(number2);
        printf(" = ?");
        fflush(stdout);
        soundPlay(&Phrases[SOUND_EQUAL]);

        while (soundStatus() == SOUND_PLAYING);

        scanf("%d", &answer);

        // make sure user isn't exiting
        if (answer != 0) {
            numProblems++;

            // test if user is correct
            if (answer == (number1 + number2)) {
                soundPlay(&Phrases[SOUND_CORRECT]);

                while (soundStatus() == SOUND_PLAYING);

                numCorrect++;
            } else {
                // oops wrong answer!
                soundPlay(&Phrases[SOUND_WRONG]);

                while (soundStatus() == SOUND_PLAYING);

                sayNumber(number1 + number2);
                timeDelay(25);
            }
        } else {
            done = 1;
        }
    }

    // unload all the sounds
    for (number1 = 1; number <= 9; number++) {
        soundUnload(&Ones[number]);
    }

    for (number = 11; number <= 19; number++) {
        soundUnload(&Teens[number]);
    }

    for (number = 10; number <= 100; number += 10) {
        soundUnload(&Tens[number / 10 - 1]);
    }

    for (number = 0; number <= 4; number++) {
        soundUnload(&Phrases[number]);
    }

    // tell user their statistics
    if (numProblems > 0) {
        printf("You got %.0f percent of the problems correct.", 100 * numCorrect / numProblems);
    }
}
