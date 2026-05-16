#include <stdio.h>
#include <unistd.h>

#include "si5351_wspr.h"

/*
 * Baby-step test settings.
 *
 * Use large 100 Hz spacing first so WaveForms clearly shows movement:
 *
 *   tone 0 -> 1500 Hz audio
 *   tone 1 -> 1600 Hz audio
 *   tone 2 -> 1700 Hz audio
 *   tone 3 -> 1800 Hz audio
 *
 * Later, change TEST_SPACING_HZ to 1.46484375 for real WSPR spacing.
 */
#define MIXER_CLK_HZ      7038600.0
#define BASE_AUDIO_HZ        1500.0
#define TEST_SPACING_HZ       100.0

int main(void)
{
    si5351_init_wspr();

    printf("4-tone CLK2 baby-step test started.\n");
    printf("Probe TP6 or TP7 audio output with AD2.\n");
    printf("Expected audio tones: 1500, 1600, 1700, 1800 Hz.\n");
    printf("Press Ctrl-C to stop.\n\n");

    while (1) {
        for (int tone = 0; tone < 4; tone++) {
            double expected_audio_hz = BASE_AUDIO_HZ + tone * TEST_SPACING_HZ;
            double expected_clk2_hz  = MIXER_CLK_HZ + expected_audio_hz;

            printf("tone %d: expected CLK2 %.3f Hz, expected audio %.3f Hz\n",
                   tone, expected_clk2_hz, expected_audio_hz);
            fflush(stdout);

            si5351_set_clk2_tone(tone, BASE_AUDIO_HZ, TEST_SPACING_HZ);

            sleep(5);
        }
    }

    si5351_close();
    return 0;
}