/*
 * wspr_transmit.c
 *
 * Minimal WSPR transmit program.
 *
 * Goal:
 *
 *   Take one WSPR message from the command line:
 *
 *       CALLSIGN LOCATOR POWER
 *
 *   convert it into 162 WSPR symbols using wspr_encoder.c,
 *   then transmit those symbols by stepping Si5351 CLK2.
 *
 * Example:
 *
 *   sudo ./wspr_transmit VK2ABC QF56 10
 *
 * This file only handles the transmit flow.
 *
 * It does not:
 *
 *   - encode WSPR messages internally
 *   - contain the WSPR sync vector
 *   - contain convolution encoder code
 *   - contain interleaver code
 *   - contain Si5351 register tables
 *
 * File responsibilities:
 *
 *   wspr_encoder.c   -> CALLSIGN LOCATOR POWER to symbols[162]
 *   wspr_transmit.c  -> symbols[162] to timed CLK2 frequency steps
 *   si5351_wspr.c    -> Si5351 I2C/register work
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "wspr_encoder.h"
#include "si5351_wspr.h"

/*
 * Print the required command format.
 *
 * This is only for user guidance when the program is started incorrectly.
 */
static void usage(const char *prog)
{
    printf("Usage:\n");
    printf("  sudo %s CALLSIGN LOCATOR POWER\n", prog);
    printf("\n");
    printf("Examples:\n");
    printf("  sudo %s VK2JPG QF56 3\n", prog);
    printf("  sudo %s VK2ABC QF56 10\n", prog);
}

int main(int argc, char **argv)
{
    /*
     * Expected command:
     *
     *   sudo ./wspr_transmit CALLSIGN LOCATOR POWER
     *
     * Example:
     *
     *   sudo ./wspr_transmit VK2ABC QF56 10
     */
    if (argc != 4) {
        usage(argv[0]);
        return 1;
    }

    /*
     * Read the WSPR message fields from the command line.
     *
     * Assumption:
     *
     *   The user provides correct uppercase WSPR input.
     *
     * Example:
     *
     *   callsign = "VK2ABC"
     *   locator  = "QF56"
     *   power    = 10
     */
    const char *callsign = argv[1];
    const char *locator  = argv[2];
    int power = atoi(argv[3]);

    /*
     * Storage for the 162 WSPR tone symbols.
     *
     * Each symbol will be:
     *
     *   0, 1, 2, or 3
     *
     * The encoder helper fills this array.
     */
    uint8_t symbols[WSPR_SYMBOLS];

    /*
     * Convert the message into WSPR symbols.
     *
     * The encoder is now separated into:
     *
     *   wspr_encoder.c
     *
     * So this transmit file does not need to know how the WSPR message is
     * packed, convolution encoded, interleaved, or combined with sync bits.
     */
    encode_wspr(callsign, locator, power, symbols);

    /*
     * RF frequency plan for CLK2.
     *
     * For the current hardwired loopback:
     *
     *   CLK2 is the RF-like signal injected into the receiver path.
     *
     * Tone 0 starts at:
     *
     *   7040100 Hz
     *
     * WSPR tone spacing is:
     *
     *   12000 / 8192 = 1.46484375 Hz
     *
     * Therefore:
     *
     *   tone 0 -> 7040100.0000 Hz
     *   tone 1 -> 7040101.4648 Hz
     *   tone 2 -> 7040102.9297 Hz
     *   tone 3 -> 7040104.3945 Hz
     */
    const double base_hz = 7040100.0;
    const double tone_spacing_hz = 12000.0 / 8192.0;

    /*
     * WSPR symbol duration.
     *
     * Exact WSPR symbol time is:
     *
     *   8192 / 12000 seconds
     *   about 0.682666 seconds
     *
     * This simple project uses:
     *
     *   683000 microseconds
     *
     * which is close enough for this baby-step loopback test.
     */
    const useconds_t symbol_us = 683000;

    /*
     * Print the transmit setup.
     *
     * This is useful because the shell script saves this output into the
     * transmit log file.
     */
    printf("WSPR transmit started\n");
    printf("Message: %s %s %d\n", callsign, locator, power);
    printf("Base CLK2: %.4f Hz\n", base_hz);
    printf("Tone spacing: %.8f Hz\n", tone_spacing_hz);
    printf("Symbol time: %u us\n", (unsigned)symbol_us);
    printf("Symbols: %u\n", WSPR_SYMBOLS);

    /*
     * Initialize the Si5351 once before the 162-symbol transmit loop.
     *
     * si5351_wspr.c handles the actual I2C/register setup.
     *
     * After this startup step, this program only updates CLK2.
     */
    si5351_init_wspr();

    /*
     * Transmit all 162 WSPR symbols.
     *
     * For each symbol:
     *
     *   1. Read the tone number from symbols[].
     *   2. Convert tone number to CLK2 frequency.
     *   3. Write that frequency to the Si5351.
     *   4. Hold it for one WSPR symbol time.
     */
    for (unsigned i = 0; i < WSPR_SYMBOLS; i++) {
        uint8_t tone = symbols[i];
        double freq_hz = base_hz + tone * tone_spacing_hz;

        printf("symbol %u: tone %u, CLK2 %.4f Hz\n",
               i, tone, freq_hz);

        si5351_set_clk2_freq_hz(freq_hz);

        usleep(symbol_us);
    }

    /*
     * Close the Si5351 I2C connection after transmission finishes.
     */
    si5351_close();

    printf("Done\n");
    return 0;
}