/*
 * wspr_encoder.c
 *
 * WSPR message encoder.
 *
 * Goal:
 *
 *   Convert one normal WSPR message:
 *
 *       CALLSIGN LOCATOR POWER
 *
 *   into the 162 WSPR tone symbols needed by the transmitter.
 *
 * Example input:
 *
 *       VK2ABC QF56 10
 *
 * Output:
 *
 *       symbols[162]
 *
 *   Each symbol is one tone number:
 *
 *       0, 1, 2, or 3
 *
 * This file only handles message encoding.
 *
 * It does not:
 *
 *   - transmit
 *   - wait for WSPR time slots
 *   - configure the Si5351
 *   - write I2C registers
 *   - validate user input
 *
 * Assumption:
 *
 *   The caller provides correct uppercase WSPR input.
 */

#include <stdint.h>
#include <string.h>

#include "wspr_encoder.h"

/*
 * WSPR convolution encoder polynomials.
 *
 * These constants define how the 50-bit message plus tail bits are expanded
 * into 162 coded bits.
 */
#define POLY1 0xF2D05351u
#define POLY2 0xE4613C47u

/*
 * WSPR synchronization vector.
 *
 * The sync vector is mixed with the encoded data bits to produce the final
 * 4-tone WSPR symbol sequence.
 *
 * Final symbol rule:
 *
 *     symbol = sync_bit + 2 * data_bit
 *
 * So each final symbol becomes:
 *
 *     0, 1, 2, or 3
 */
static const uint8_t SYNC_VECTOR[WSPR_SYMBOLS] = {
    1,1,0,0,0,0,0,0,1,0,0,0,1,1,1,0,0,0,1,0,0,1,0,1,1,1,1,0,0,0,0,0,
    0,0,1,0,0,1,0,1,0,0,0,0,0,0,1,0,1,1,0,0,1,1,0,1,0,0,0,1,1,0,1,0,
    0,0,0,1,1,0,1,0,1,0,1,0,1,0,0,1,0,0,1,0,1,1,0,0,0,1,1,0,1,0,1,0,
    0,0,1,0,0,0,0,0,1,0,0,1,0,0,1,1,1,0,1,1,0,0,1,1,0,1,0,0,0,1,1,1,
    0,0,0,0,0,1,0,1,0,0,1,1,0,0,0,0,0,0,0,1,1,0,1,0,1,1,0,0,0,1,1,0,
    0,0
};

/*
 * WSPR character table.
 *
 * WSPR does not store callsign characters as ASCII directly.
 * It first converts each callsign character into an index from this table.
 *
 * Index range:
 *
 *     0-9    digits
 *     10-35  letters A-Z
 *     36     space
 */
static const char CHARS[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

/*
 * Return the WSPR table index for one character.
 *
 * The input is assumed valid.
 * This keeps the encoder small.
 */
static int char_index(char c)
{
    return (int)(strchr(CHARS, c) - CHARS);
}

/*
 * Pack the callsign into the WSPR callsign number.
 *
 * WSPR uses a fixed 6-character callsign field.
 *
 * Examples:
 *
 *     VK2ABC -> "VK2ABC"
 *     K1ABC  -> " K1ABC"
 *
 * The leading space case is needed when the second callsign character is
 * a digit.
 */
static uint32_t encode_callsign(const char *call_in)
{
    /*
     * Start with six spaces.
     *
     * Shorter callsigns automatically remain space-padded.
     */
    char call[7] = "      ";

    /*
     * Handle callsigns like K1ABC.
     *
     * These are shifted right by one position and given a leading space.
     */
    if (call_in[1] >= '0' && call_in[1] <= '9') {
        call[0] = ' ';

        for (int i = 0; call_in[i] && i < 5; i++) {
            call[i + 1] = call_in[i];
        }
    }

    /*
     * Normal callsigns like VK2ABC are copied directly into the 6-character
     * field.
     */
    else {
        for (int i = 0; call_in[i] && i < 6; i++) {
            call[i] = call_in[i];
        }
    }

    /*
     * Compress the 6-character callsign into one WSPR number.
     */
    uint32_t n;

    n = char_index(call[0]);
    n = n * 36 + char_index(call[1]);
    n = n * 10 + char_index(call[2]);
    n = n * 27 + (char_index(call[3]) - 10);
    n = n * 27 + (char_index(call[4]) - 10);
    n = n * 27 + (char_index(call[5]) - 10);

    return n;
}

/*
 * Pack the 4-character Maidenhead locator and power level.
 *
 * Example:
 *
 *     QF56 10
 *
 * The locator and power are packed together into one WSPR number.
 */
static uint32_t encode_locator_power(const char *loc, int pwr)
{
    uint32_t m;

    /*
     * Compress the 4-character locator.
     */
    m  = (179 - 10 * (loc[0] - 'A') - (loc[2] - '0')) * 180;
    m += 10 * (loc[1] - 'A') + (loc[3] - '0');

    /*
     * Add power into the packed locator/power field.
     */
    return m * 128 + pwr + 64;
}

/*
 * Build the encoder input bit stream.
 *
 * WSPR starts with:
 *
 *     28 bits callsign
 *     22 bits locator/power
 *
 * Together this is 50 message bits.
 *
 * Then 31 zero tail bits are added for the convolution encoder.
 *
 * Output:
 *
 *     bits[81]
 */
static void pack_50_bits(uint32_t n, uint32_t m, uint8_t bits[81])
{
    uint64_t combined = ((uint64_t)n << 22) | m;

    /*
     * Extract the 50 message bits, most-significant bit first.
     */
    for (int i = 0; i < 50; i++) {
        bits[i] = (combined >> (49 - i)) & 1;
    }

    /*
     * Add 31 zero tail bits.
     */
    for (int i = 50; i < 81; i++) {
        bits[i] = 0;
    }
}

/*
 * Calculate bit parity.
 *
 * Returns:
 *
 *     0 if the number of 1 bits is even
 *     1 if the number of 1 bits is odd
 *
 * The convolution encoder uses this to produce each coded bit.
 */
static uint8_t parity(uint32_t x)
{
    x ^= x >> 16;
    x ^= x >> 8;
    x ^= x >> 4;
    x ^= x >> 2;
    x ^= x >> 1;

    return x & 1;
}

/*
 * Convolution encoder.
 *
 * Input:
 *
 *     81 bits
 *
 * Output:
 *
 *     162 coded bits
 *
 * Each input bit produces two output bits.
 */
static void conv_encode(const uint8_t bits[81], uint8_t out[WSPR_SYMBOLS])
{
    uint32_t reg = 0;

    for (int i = 0; i < 81; i++) {
        /*
         * Shift the next message bit into the encoder register.
         */
        reg = (reg << 1) | bits[i];

        /*
         * Generate two coded bits from the current register state.
         */
        out[2 * i]     = parity(reg & POLY1);
        out[2 * i + 1] = parity(reg & POLY2);
    }
}

/*
 * Reverse the bit order of an 8-bit value.
 *
 * This is used to calculate the interleaver write positions.
 */
static uint8_t bit_reverse_8(uint8_t i)
{
    uint8_t out = 0;

    for (int n = 0; n < 8; n++) {
        out = (out << 1) | (i & 1);
        i >>= 1;
    }

    return out;
}

/*
 * Interleave the coded bits.
 *
 * The interleaver spreads nearby coded bits across different positions.
 *
 * Input:
 *
 *     162 coded bits
 *
 * Output:
 *
 *     162 interleaved bits
 */
static void interleave(const uint8_t bits[WSPR_SYMBOLS],
                       uint8_t dest[WSPR_SYMBOLS])
{
    int p = 0;

    for (int i = 0; i < 256; i++) {
        int j = bit_reverse_8(i);

        /*
         * Only use bit-reversed positions inside the 162-symbol WSPR frame.
         */
        if (j < WSPR_SYMBOLS) {
            dest[j] = bits[p];
            p++;

            if (p == WSPR_SYMBOLS) {
                break;
            }
        }
    }
}

/*
 * Encode one complete WSPR message.
 *
 * Input:
 *
 *     callsign
 *     locator
 *     power
 *
 * Output:
 *
 *     symbols[162]
 *
 * This is the only function that wspr_transmit.c needs to call.
 */
void encode_wspr(const char *callsign,
                 const char *locator,
                 int power,
                 uint8_t symbols[WSPR_SYMBOLS])
{
    /*
     * Compress the message fields.
     */
    uint32_t n = encode_callsign(callsign);
    uint32_t m = encode_locator_power(locator, power);

    /*
     * Temporary arrays for each encoding stage.
     */
    uint8_t bits[81];
    uint8_t coded[WSPR_SYMBOLS];
    uint8_t interl[WSPR_SYMBOLS];

    /*
     * Build the 81-bit encoder input.
     */
    pack_50_bits(n, m, bits);

    /*
     * Convert 81 bits into 162 coded bits.
     */
    conv_encode(bits, coded);

    /*
     * Interleave the coded bits.
     */
    interleave(coded, interl);

    /*
     * Combine sync and data into final WSPR symbols.
     */
    for (int i = 0; i < WSPR_SYMBOLS; i++) {
        symbols[i] = SYNC_VECTOR[i] + 2 * interl[i];
    }
}