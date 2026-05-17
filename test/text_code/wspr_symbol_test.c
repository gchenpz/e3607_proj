/*
 * wspr_symbol_test.c
 *
 * Tiny WSPR-style symbol stepping test.
 *
 * Purpose:
 *   Prove that the Si5351 CLK2 output can step through WSPR-like
 *   4-FSK symbols using the real WSPR tone spacing.
 *
 * This is NOT the full WSPR encoder yet.
 * The symbols[] array below is only dummy test data.
 *
 * Hardware path in our setup:
 *   AUP-ZU3 Linux program
 *        -> I2C
 *        -> Si5351
 *        -> CLK2 RF output
 *        -> attenuators
 *        -> WSPR-SDR RF/mixer/audio path
 *        -> AD2 / USB audio check
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include "si5351_wspr.h"

/*
 * Dummy symbol sequence.
 *
 * Real WSPR produces 162 symbols.
 * Each symbol is one of:
 *
 *   0, 1, 2, 3
 *
 * Those four values select the four FSK tones.
 *
 * For this first test, we only use a short repeating pattern so that
 * we can verify symbol stepping without worrying about the encoder yet.
 */
 
// static const uint8_t symbols[] = {
//     0, 1, 2, 3,
//     0, 1, 2, 3,
//     3, 2, 1, 0,
//     3, 2, 1, 0
// };

/*
  * WSPR symbols for:
  *   callsign = VK2JPG
  *   locator  = QF56
  *   power    = 3
  */
static const uint8_t symbols[162] = {
    3, 3, 2, 0, 0, 0, 2, 2, 1, 2, 2, 0, 3, 1, 1, 2, 2, 0,
    3, 2, 0, 3, 2, 3, 3, 1, 3, 2, 2, 2, 0, 0, 0, 2, 1, 2,
    2, 3, 2, 3, 2, 2, 2, 2, 0, 0, 1, 2, 3, 3, 0, 2, 3, 1,
    0, 1, 2, 0, 0, 3, 3, 2, 1, 2, 2, 0, 2, 3, 1, 0, 1, 0,
    1, 2, 1, 2, 1, 0, 2, 1, 2, 0, 1, 0, 1, 1, 0, 0, 2, 1,
    3, 2, 1, 2, 3, 0, 0, 0, 1, 0, 2, 0, 0, 0, 3, 0, 2, 3,
    2, 2, 1, 1, 1, 2, 3, 1, 0, 0, 3, 3, 2, 3, 0, 0, 2, 1,
    1, 3, 2, 0, 2, 2, 2, 3, 2, 3, 2, 2, 3, 1, 2, 0, 0, 0,
    0, 2, 2, 3, 3, 2, 3, 0, 3, 1, 2, 0, 0, 3, 1, 0, 0, 0,
};

int main(void)
{
    /*
     * Base RF frequency for CLK2.
     *
     * This is the RF tone that should produce about 1500 Hz audio
     * after mixing in our current loopback receiver setup.
     *
     * Your previous test used:
     *
     *   CLK2 = 7040100 Hz -> audio about 1500 Hz
     *
     * So symbol 0 starts here.
     */
    const double base_hz = 7040100.0;

    /*
     * WSPR tone spacing.
     *
     * WSPR uses 4-FSK.
     * Adjacent tones are separated by about 1.4648 Hz.
     *
     * Therefore:
     *
     *   symbol 0 -> base_hz + 0 * tone_spacing_hz
     *   symbol 1 -> base_hz + 1 * tone_spacing_hz
     *   symbol 2 -> base_hz + 2 * tone_spacing_hz
     *   symbol 3 -> base_hz + 3 * tone_spacing_hz
     */
    const double tone_spacing_hz = 12000.0 / 8192.0;

    /*
     * WSPR symbol time.
     *
     * Symbol rate = 12000 / 8192 = 1.46484375 symbols/second.
     *
     * Symbol time = 1 / symbol_rate
     *             = 8192 / 12000
     *             = 0.682666... seconds
     *
     * usleep() uses microseconds, so:
     *
     *   0.682666 s ≈ 682667 us
     *
     * 683000 us is close enough for this hardware stepping test.
     */
    const useconds_t symbol_us = 683000;

    /*
     * Configure the Si5351 fixed setup once.
     *
     * This should set up the PLLs, CLK0/CLK1 mixer clocks if your
     * si5351_wspr.c does that, and prepare CLK2 for updates.

     * In our current simple driver, si5351_init_wspr() returns void,
     * so we just call it directly.
     */
    si5351_init_wspr();

    printf("WSPR symbol stepping test started\n");
    printf("Base CLK2: %.4f Hz\n", base_hz);
    printf("Tone spacing: %.8f Hz\n", tone_spacing_hz);
    printf("Symbol time: %u us\n", symbol_us);

    /*
     * Step through each test symbol.
     *
     * For each symbol:
     *   1. Read symbol value 0..3
     *   2. Convert it to an RF frequency
     *   3. Program CLK2 to that frequency
     *   4. Hold it for one WSPR symbol period
     */
    for (unsigned i = 0; i < sizeof(symbols) / sizeof(symbols[0]); i++) {
        uint8_t tone = symbols[i];

        /*
         * Convert 4-FSK symbol value into RF frequency.
         *
         * Example:
         *   tone = 0 -> 7040100.0000 Hz
         *   tone = 1 -> 7040101.4648 Hz
         *   tone = 2 -> 7040102.9297 Hz
         *   tone = 3 -> 7040104.3945 Hz
         */
        double freq_hz = base_hz + tone * tone_spacing_hz;

        printf("symbol %u: tone %u, CLK2 %.4f Hz\n",
               i, tone, freq_hz);

        /*
         * Low-level CLK2 frequency update.
         *
         * This is the important action being tested.
         */
        si5351_set_clk2_freq_hz(freq_hz);

        /*
         * Hold this frequency for one WSPR symbol period.
         */
        usleep(symbol_us);
    }

    si5351_close();

    printf("Done\n");
    return 0;
}