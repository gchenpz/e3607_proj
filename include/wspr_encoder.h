#ifndef WSPR_ENCODER_H
#define WSPR_ENCODER_H

#include <stdint.h>

#define WSPR_SYMBOLS 162

void encode_wspr(const char *callsign,
                 const char *locator,
                 int power,
                 uint8_t symbols[WSPR_SYMBOLS]);

#endif