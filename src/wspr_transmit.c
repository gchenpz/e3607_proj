/*
 * wspr_transmit.c
 *
 * Baby-step WSPR transmitter command.
 *
 * This version accepts a normal WSPR message at runtime:
 *
 *   sudo ./wspr_transmit CALLSIGN LOCATOR POWER
 *
 * Example:
 *
 *   sudo ./wspr_transmit VK2JPG QF56 3
 *   sudo ./wspr_transmit VK2ABC QF56 10
 *
 * What this file does:
 *
 *   1. Reads CALLSIGN, LOCATOR, and POWER from the command line.
 *   2. Encodes them into the standard 162 WSPR symbols.
 *   3. Starts the Si5351 WSPR clock setup.
 *   4. Steps CLK2 through the 4 WSPR tones.
 *
 * What this file does NOT do:
 *
 *   - It does not change CLK0 or CLK1 directly.
 *   - It does not rewrite the Si5351 driver.
 *   - It does not use the old hardcoded VK2JPG symbol table.
 *
 * The Si5351 details stay inside:
 *
 *   si5351_wspr.c
 *   si5351_wspr.h
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#include "si5351_wspr.h"

/*
 * WSPR always sends 162 symbols.
 *
 * Each symbol is one of:
 *
 *   0, 1, 2, or 3
 *
 * Those four values select the four WSPR FSK tones.
 */
#define WSPR_SYMBOLS 162

/*
 * WSPR convolution encoder polynomials.
 *
 * These are the same values used in your Python encoder.
 */
#define POLY1 0xF2D05351u
#define POLY2 0xE4613C47u

/*
 * Standard WSPR sync vector.
 *
 * Final transmitted symbol:
 *
 *   symbol = sync_bit + 2 * data_bit
 *
 * So the final symbol is 0, 1, 2, or 3.
 */
static const uint8_t sync_vector[WSPR_SYMBOLS] = {
    1,1,0,0,0,0,0,0,1,0,0,0,1,1,1,0,0,0,1,0,0,1,0,1,1,1,1,0,0,0,0,0,
    0,0,1,0,0,1,0,1,0,0,0,0,0,0,1,0,1,1,0,0,1,1,0,1,0,0,0,1,1,0,1,0,
    0,0,0,1,1,0,1,0,1,0,1,0,1,0,0,1,0,0,1,0,1,1,0,0,0,1,1,0,1,0,1,0,
    0,0,1,0,0,0,0,0,1,0,0,1,0,0,1,1,1,0,1,1,0,0,1,1,0,1,0,0,0,1,1,1,
    0,0,0,0,0,1,0,1,0,0,1,1,0,0,0,0,0,0,0,1,1,0,1,0,1,1,0,0,0,1,1,0,
    0,0
};

/*
 * WSPR character table used for callsign packing.
 *
 * Index:
 *
 *   0-9   = digits
 *   10-35 = A-Z
 *   36    = space
 */
static const char wspr_chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

/*
 * Print command-line help.
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

/*
 * Find a character in the WSPR character table.
 *
 * Returns:
 *
 *   0-36 on success
 *   -1   on invalid character
 */
static int wspr_char_index(char c)
{
    const char *p = strchr(wspr_chars, c);

    if (p == NULL) {
        return -1;
    }

    return (int)(p - wspr_chars);
}

/*
 * Copy a string while:
 *
 *   - removing leading spaces
 *   - removing trailing spaces
 *   - converting to uppercase
 *
 * This lets the user type:
 *
 *   vk2jpg qf56 3
 *
 * and still encode it as:
 *
 *   VK2JPG QF56 3
 */
static void upper_trim(char *dst, size_t dst_size, const char *src)
{
    size_t len;

    while (isspace((unsigned char)*src)) {
        src++;
    }

    len = strlen(src);

    while (len > 0 && isspace((unsigned char)src[len - 1])) {
        len--;
    }

    if (len >= dst_size) {
        len = dst_size - 1;
    }

    for (size_t i = 0; i < len; i++) {
        dst[i] = (char)toupper((unsigned char)src[i]);
    }

    dst[len] = '\0';
}

/*
 * Encode the callsign part of a WSPR message.
 *
 * Input examples:
 *
 *   VK2JPG
 *   VK2ABC
 *   K1ABC
 *
 * WSPR internally stores the callsign as 6 characters.
 *
 * Important simple rule:
 *
 *   If the second callsign character is a digit, WSPR adds a leading space.
 *
 * Example:
 *
 *   K1ABC  becomes " K1ABC"
 *   VK2JPG stays    "VK2JPG"
 */
static int encode_callsign(const char *input, uint32_t *out)
{
    char tmp[16];
    char call[7] = "      ";

    upper_trim(tmp, sizeof(tmp), input);

    if (strlen(tmp) == 0 || strlen(tmp) > 6) {
        return -1;
    }

    int dst = 0;

    /*
     * Add WSPR leading space for callsigns like:
     *
     *   K1ABC
     *   M0ABC
     */
    if (strlen(tmp) >= 2 && isdigit((unsigned char)tmp[1])) {
        call[dst++] = ' ';
    }

    /*
     * Copy callsign into 6-character WSPR field.
     * Remaining characters stay as spaces.
     */
    for (int i = 0; tmp[i] != '\0' && dst < 6; i++) {
        call[dst++] = tmp[i];
    }

    /*
     * Convert each callsign character to its WSPR index.
     */
    int c0 = wspr_char_index(call[0]);
    int c1 = wspr_char_index(call[1]);
    int c2 = wspr_char_index(call[2]);
    int c3 = wspr_char_index(call[3]);
    int c4 = wspr_char_index(call[4]);
    int c5 = wspr_char_index(call[5]);

    /*
     * Basic standard-message validation.
     *
     * WSPR compressed callsign format expects:
     *
     *   char 0: digit, letter, or space
     *   char 1: digit or letter
     *   char 2: digit
     *   char 3: letter or space
     *   char 4: letter or space
     *   char 5: letter or space
     */
    if (c0 < 0 || c1 < 0 || c2 < 0 || c3 < 0 || c4 < 0 || c5 < 0) {
        return -1;
    }

    if (c1 > 35) {
        return -1;
    }

    if (c2 > 9) {
        return -1;
    }

    if (c3 < 10 || c4 < 10 || c5 < 10) {
        return -1;
    }

    /*
     * WSPR callsign packing.
     */
    uint32_t n = (uint32_t)c0;

    n = n * 36u + (uint32_t)c1;
    n = n * 10u + (uint32_t)c2;
    n = n * 27u + (uint32_t)(c3 - 10);
    n = n * 27u + (uint32_t)(c4 - 10);
    n = n * 27u + (uint32_t)(c5 - 10);

    *out = n;
    return 0;
}

/*
 * Encode the locator and power part of a WSPR message.
 *
 * Locator example:
 *
 *   QF56
 *
 * Power example:
 *
 *   3
 *
 * For this simple project, we accept power values 0 to 60 dBm.
 */
static int encode_locator_power(const char *input, int power, uint32_t *out)
{
    char loc[8];

    upper_trim(loc, sizeof(loc), input);

    if (strlen(loc) != 4) {
        return -1;
    }

    /*
     * Maidenhead locator format used here:
     *
     *   letter letter digit digit
     *
     * Example:
     *
     *   QF56
     */
    if (!isalpha((unsigned char)loc[0]) ||
        !isalpha((unsigned char)loc[1]) ||
        !isdigit((unsigned char)loc[2]) ||
        !isdigit((unsigned char)loc[3])) {
        return -1;
    }

    /*
     * First two Maidenhead letters should be A-R.
     */
    if (loc[0] < 'A' || loc[0] > 'R' ||
        loc[1] < 'A' || loc[1] > 'R') {
        return -1;
    }

    if (power < 0 || power > 60) {
        return -1;
    }

    /*
     * WSPR locator/power packing.
     */
    uint32_t m = 0;

    m  = (uint32_t)(179 - 10 * (loc[0] - 'A') - (loc[2] - '0')) * 180u;
    m += (uint32_t)(10 * (loc[1] - 'A') + (loc[3] - '0'));

    m = m * 128u + (uint32_t)power + 64u;

    *out = m;
    return 0;
}

/*
 * Return parity of a 32-bit value.
 *
 * The convolution encoder uses this to calculate one output bit.
 */
static uint8_t parity32(uint32_t x)
{
    x ^= x >> 16;
    x ^= x >> 8;
    x ^= x >> 4;
    x ^= x >> 2;
    x ^= x >> 1;

    return (uint8_t)(x & 1u);
}

/*
 * Reverse the bits in an 8-bit number.
 *
 * This is used by the WSPR interleaver.
 */
static uint8_t bit_reverse_8(uint8_t x)
{
    uint8_t y = 0;

    for (int i = 0; i < 8; i++) {
        y = (uint8_t)((y << 1) | (x & 1u));
        x >>= 1;
    }

    return y;
}

/*
 * Encode a complete standard WSPR message.
 *
 * Inputs:
 *
 *   callsign
 *   locator
 *   power
 *
 * Output:
 *
 *   symbols[162]
 *
 * Each output symbol is:
 *
 *   0, 1, 2, or 3
 */
static int encode_wspr(const char *callsign,
                       const char *locator,
                       int power,
                       uint8_t symbols[WSPR_SYMBOLS])
{
    uint32_t n;
    uint32_t m;

    /*
     * Step 1:
     * Pack callsign into N.
     */
    if (encode_callsign(callsign, &n) != 0) {
        return -1;
    }

    /*
     * Step 2:
     * Pack locator and power into M.
     */
    if (encode_locator_power(locator, power, &m) != 0) {
        return -1;
    }

    /*
     * Step 3:
     * Build the 50-bit WSPR message:
     *
     *   28 bits callsign
     *   22 bits locator/power
     *
     * Then append 31 zero tail bits for the convolution encoder.
     */
    uint8_t bits[81];
    uint64_t packed = ((uint64_t)n << 22) | (uint64_t)m;

    for (int i = 0; i < 50; i++) {
        bits[i] = (uint8_t)((packed >> (49 - i)) & 1u);
    }

    for (int i = 50; i < 81; i++) {
        bits[i] = 0;
    }

    /*
     * Step 4:
     * Convolution encode the 81 bits into 162 coded bits.
     */
    uint8_t coded[WSPR_SYMBOLS];
    uint32_t reg = 0;

    for (int i = 0; i < 81; i++) {
        reg = (reg << 1) | bits[i];

        coded[2 * i]     = parity32(reg & POLY1);
        coded[2 * i + 1] = parity32(reg & POLY2);
    }

    /*
     * Step 5:
     * Interleave the 162 coded bits.
     */
    uint8_t interleaved[WSPR_SYMBOLS];
    int p = 0;

    for (int i = 0; i < 256; i++) {
        int j = bit_reverse_8((uint8_t)i);

        if (j < WSPR_SYMBOLS) {
            interleaved[j] = coded[p];
            p++;

            if (p == WSPR_SYMBOLS) {
                break;
            }
        }
    }

    /*
     * Step 6:
     * Combine sync vector and data bits.
     *
     * Final WSPR tone:
     *
     *   tone = sync + 2 * data
     */
    for (int i = 0; i < WSPR_SYMBOLS; i++) {
        symbols[i] = (uint8_t)(sync_vector[i] + 2u * interleaved[i]);
    }

    return 0;
}

int main(int argc, char **argv)
{
    /*
     * Command line:
     *
     *   sudo ./wspr_transmit CALLSIGN LOCATOR POWER
     *
     * Example:
     *
     *   sudo ./wspr_transmit VK2JPG QF56 3
     */
    if (argc != 4) {
        usage(argv[0]);
        return 1;
    }

    const char *callsign = argv[1];
    const char *locator  = argv[2];
    int power = atoi(argv[3]);

    /*
     * Generate the 162 WSPR symbols from the message.
     *
     * This replaces the old hardcoded:
     *
     *   static const uint8_t symbols[162] = { ... };
     */
    uint8_t symbols[WSPR_SYMBOLS];

    if (encode_wspr(callsign, locator, power, symbols) != 0) {
        printf("Bad WSPR message.\n");
        printf("\n");
        printf("Use standard format:\n");
        printf("  CALLSIGN LOCATOR POWER\n");
        printf("\n");
        printf("Example:\n");
        printf("  VK2JPG QF56 3\n");
        return 1;
    }

    /*
     * RF frequency plan.
     *
     * Tone 0 starts at 7040100 Hz.
     *
     * WSPR tone spacing:
     *
     *   12000 / 8192 = 1.46484375 Hz
     *
     * Tone frequencies:
     *
     *   tone 0 = base
     *   tone 1 = base + 1.46484375 Hz
     *   tone 2 = base + 2.9296875 Hz
     *   tone 3 = base + 4.39453125 Hz
     *
     * For the current hardwired-loopback test, CLK2 is the RF signal being
     * stepped through these four WSPR tones.
     */
    const double base_hz = 7040100.0;
    const double tone_spacing_hz = 12000.0 / 8192.0;

    /*
     * WSPR symbol length.
     *
     * Ideal value is about:
     *
     *   8192 / 12000 = 0.682666... seconds
     *
     * This simple code uses 683000 us, which is close enough for the current
     * baby-step hardware test.
     */
    const useconds_t symbol_us = 683000;

    printf("WSPR transmit started\n");
    printf("Message: %s %s %d\n", callsign, locator, power);
    printf("Base CLK2: %.4f Hz\n", base_hz);
    printf("Tone spacing: %.8f Hz\n", tone_spacing_hz);
    printf("Symbol time: %u us\n", (unsigned)symbol_us);
    printf("Symbols: %u\n", WSPR_SYMBOLS);

    /*
     * Configure the Si5351 once.
     *
     * Your si5351_wspr.c currently does the full startup setup:
     *
     *   CLK0 fixed mixer clock
     *   CLK1 fixed mixer clock
     *   CLK2 RF clock
     *
     * After this, the loop below only updates CLK2.
     */
    si5351_init_wspr();

    /*
     * Step through the 162 WSPR symbols.
     *
     * For each symbol:
     *
     *   1. Convert tone number 0-3 into CLK2 frequency.
     *   2. Write the new CLK2 frequency to the Si5351.
     *   3. Hold that tone for one WSPR symbol time.
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
     * Close the I2C device.
     */
    si5351_close();

    printf("Done\n");
    return 0;
}