#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>

#include "Si5351A-RevB-Registers.h"
#include "si5351_wspr.h"

#define I2C_FNAME   "/dev/i2c-3"
#define SI5351_ADDR 0x60

/*
 * Fixed clock plan.
 *
 * CLK0 and CLK1 are fixed mixer clocks.
 * CLK2 is the RF carrier that we change at runtime.
 */
#define PLLA_FREQ_HZ 893902200.0
#define MIXER_CLK_HZ 7038600.0

static int i2c_file = -1;

static void die(const char *msg)
{
    perror(msg);
    exit(1);
}

static void write_reg(uint8_t reg, uint8_t val)
{
    if (i2c_smbus_write_byte_data(i2c_file, reg, val) < 0) {
        fprintf(stderr, "I2C write failed: reg 0x%02X <- 0x%02X\n", reg, val);
        die("i2c_smbus_write_byte_data");
    }
}

/*
 * JOB 1:
 * Full Si5351 startup.
 *
 * This writes the known-good full hardware clock infrastructure:
 *
 *   CLK0 = 7.0386 MHz
 *   CLK1 = 7.0386 MHz
 *   CLK2 = 7.0401 MHz base
 *
 * This replaces the need to run ./i2cwrite separately.
 */
void si5351_init_wspr(void)
{
    i2c_file = open(I2C_FNAME, O_RDWR);
    if (i2c_file < 0) {
        die("open /dev/i2c-3");
    }

    if (ioctl(i2c_file, I2C_SLAVE, SI5351_ADDR) < 0) {
        die("ioctl I2C_SLAVE");
    }

    /*
     * Safe startup sequence.
     */

    /* 1. Disable all outputs. */
    write_reg(0x03, 0xFF);

    /* 2. Power down all output drivers. */
    for (uint8_t reg = 0x10; reg <= 0x17; reg++) {
        write_reg(reg, 0x80);
    }

    /* 3. Write fixed startup register table. */
    for (int i = 0; i < SI5351A_REVB_REG_CONFIG_NUM_REGS; i++) {
        write_reg(si5351a_revb_registers[i].address,
                  si5351a_revb_registers[i].value);
    }

    /* 4. Soft reset PLLA and PLLB. */
    write_reg(0xB1, 0xAC);

    /* 5. Give PLLs a brief moment to settle. */
    usleep(1000);

    /* 6. Enable outputs. */
    write_reg(0x03, 0x00);
}

/*
 * JOB 2:
 * Runtime CLK2 frequency update.
 *
 * This function changes ONLY CLK2 MultiSynth2 registers:
 *
 *   0x3A–0x41
 *
 * It does not touch CLK0.
 * It does not touch CLK1.
 * It does not reset PLLA/PLLB.
 * It does not rewrite the full startup table.
 */
void si5351_set_clk2_freq_hz(double clk2_hz)
{
    /*
     * MultiSynth divider:
     *
     *   divider = PLLA_FREQ_HZ / clk2_hz
     *   divider = a + b/c
     *
     * Use c = 1,000,000 for enough fractional resolution.
     */
    const uint32_t c = 1000000;

    double div = PLLA_FREQ_HZ / clk2_hz;
    uint32_t a = (uint32_t)div;
    uint32_t b = (uint32_t)((div - (double)a) * c + 0.5);

    /*
     * Convert a,b,c into Si5351 MultiSynth parameters.
     *
     *   P1 = 128a + floor(128b/c) - 512
     *   P2 = 128b - c × floor(128b/c)
     *   P3 = c
     */
    uint32_t x  = (128 * b) / c;
    uint32_t p1 = 128 * a + x - 512;
    uint32_t p2 = 128 * b - c * x;
    uint32_t p3 = c;

    /*
     * Pack P1/P2/P3 into MultiSynth2 registers.
     *
     * Register block:
     *
     *   0x3A–0x41 = CLK2 MultiSynth2
     */
    write_reg(0x3A, (p3 >> 8) & 0xFF);
    write_reg(0x3B,  p3       & 0xFF);
    write_reg(0x3C, (p1 >> 16) & 0x03);
    write_reg(0x3D, (p1 >> 8)  & 0xFF);
    write_reg(0x3E,  p1        & 0xFF);
    write_reg(0x3F, (((p3 >> 16) & 0x0F) << 4) | ((p2 >> 16) & 0x0F));
    write_reg(0x40, (p2 >> 8) & 0xFF);
    write_reg(0x41,  p2       & 0xFF);
}

/*
 * Runtime tone-number interface.
 *
 * This function converts a tone number into the required CLK2 RF frequency,
 * then calls si5351_set_clk2_freq_hz() to update only CLK2.
 *
 * This is the main clean interface used by:
 *
 *   tone_step_test.c now
 *   WSPR transmit loop later
 *
 * Mixer relationship:
 *
 *   audio_hz = CLK2 - MIXER_CLK_HZ
 *
 * Therefore:
 *
 *   CLK2 = MIXER_CLK_HZ + audio_hz
 *
 * Tone relationship:
 *
 *   audio_hz = base_audio_hz + tone * spacing_hz
 *
 * So:
 *
 *   CLK2 = MIXER_CLK_HZ + base_audio_hz + tone * spacing_hz
 *
 * For the baby-step test:
 *
 *   base_audio_hz = 1500.0
 *   spacing_hz    = 100.0
 *
 * Later for real WSPR:
 *
 *   base_audio_hz = 1500.0
 *   spacing_hz    = 1.46484375
 *
 * This function does not touch CLK0.
 * This function does not touch CLK1.
 * This function does not reset PLLA/PLLB.
 * This function does not rewrite the full startup table.
 */
void si5351_set_clk2_tone(int tone, double base_audio_hz, double spacing_hz)
{
    double audio_hz = base_audio_hz + tone * spacing_hz;
    double clk2_hz  = MIXER_CLK_HZ + audio_hz;

    si5351_set_clk2_freq_hz(clk2_hz);
}

void si5351_close(void)
{
    if (i2c_file >= 0) {
        close(i2c_file);
        i2c_file = -1;
    }
}