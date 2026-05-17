#ifndef SI5351A_REVB_REGISTERS_H
#define SI5351A_REVB_REGISTERS_H

#include <stdint.h>

/*
 * One Si5351 register write.
 *
 * address = Si5351 register address
 * value   = byte written to that address
 */
typedef struct {
    uint8_t address;
    uint8_t value;
} si5351a_revb_register_t;

/*
 * Si5351 startup register table for ELEC3607 WSPR loopback.
 *
 * This table represents the fixed board clock infrastructure.
 *
 * Clock plan:
 *
 *   CLK0 = 7.0386 MHz
 *      Mixer clock output.
 *      CLK0 has the programmed phase offset through CLK0_PHOFF.
 *
 *   CLK1 = 7.0386 MHz
 *      Mixer clock output.
 *      CLK1 is the zero-phase reference.
 *
 *   CLK2 ~= 7.040100 MHz
 *      Initial RF loopback carrier.
 *      Later runtime code updates only CLK2 MultiSynth2 registers
 *      0x3A–0x41 to generate test tones or WSPR tones.
 *
 * Important:
 *
 *   This table intentionally does NOT contain register 0x03.
 *
 *   Register 0x03 is the output-enable register. It should be handled
 *   by si5351_wspr.c as part of the startup sequence:
 *
 *      1. Disable outputs:              0x03 = 0xFF
 *      2. Power down output drivers:    0x10–0x17 = 0x80
 *      3. Write this startup table
 *      4. Soft-reset PLLA and PLLB:     0xB1 = 0xAC
 *      5. Wait briefly
 *      6. Enable outputs:               0x03 = 0x00
 *
 * Runtime WSPR modulation should NOT rewrite this full table.
 * Runtime modulation should only update CLK2 MultiSynth2 registers:
 *
 *      0x3A–0x41
 */

#define SI5351A_REVB_REG_CONFIG_NUM_REGS 59

static const si5351a_revb_register_t si5351a_revb_registers[SI5351A_REVB_REG_CONFIG_NUM_REGS] = {

    /*
     * General Si5351 device configuration.
     *
     * These values come from the ClockBuilder-generated,
     * board-verified configuration used for this WSPR-SDR setup.
     */
    { 0x02, 0x53 },
    { 0x04, 0x20 },
    { 0x07, 0x00 },
    { 0x0F, 0x00 },

    /*
     * Output driver control registers.
     *
     * 0x10: CLK0 enabled
     * 0x11: CLK1 enabled
     * 0x12: CLK2 enabled
     *
     * 0x13–0x17: CLK3–CLK7 unused / powered down
     */
    { 0x10, 0x0F },   /* CLK0 control: enabled */
    { 0x11, 0x0F },   /* CLK1 control: enabled */
    { 0x12, 0x0F },   /* CLK2 control: enabled */
    { 0x13, 0x8C },   /* CLK3 unused */
    { 0x14, 0x8C },   /* CLK4 unused */
    { 0x15, 0x8C },   /* CLK5 unused */
    { 0x16, 0x8C },   /* CLK6 unused */
    { 0x17, 0x8C },   /* CLK7 unused */

    /*
     * PLLA configuration.
     *
     * PLLA is the internal high-frequency source used by CLK0, CLK1,
     * and CLK2.
     *
     * This board-verified configuration uses:
     *
     *   crystal input ~= 27 MHz
     *   PLLA Fvco     ~= 893.9022 MHz
     *
     * CLK0/CLK1:
     *
     *   893.9022 MHz / 127 = 7.0386 MHz
     */
    { 0x1A, 0xAF },
    { 0x1B, 0xC8 },
    { 0x1C, 0x00 },
    { 0x1D, 0x0E },
    { 0x1E, 0x8D },
    { 0x1F, 0x00 },
    { 0x20, 0x85 },
    { 0x21, 0x58 },

    /*
     * MultiSynth0: CLK0 frequency divider.
     *
     * Output:
     *
     *   CLK0 = 7.0386 MHz
     *
     * Divider:
     *
     *   N = 127
     *
     * Decoded MultiSynth values:
     *
     *   P1 = 15744
     *   P2 = 0
     *   P3 = 1
     *
     * Equivalent divider form:
     *
     *   divider = a + b/c
     *           = 127 + 0/1
     *           = 127
     *
     * CLK0 phase is not set here. The phase offset is set later
     * using CLK0_PHOFF.
     */
    { 0x2A, 0x00 },
    { 0x2B, 0x01 },
    { 0x2C, 0x00 },
    { 0x2D, 0x3D },
    { 0x2E, 0x80 },
    { 0x2F, 0x00 },
    { 0x30, 0x00 },
    { 0x31, 0x00 },

    /*
     * MultiSynth1: CLK1 frequency divider.
     *
     * Output:
     *
     *   CLK1 = 7.0386 MHz
     *
     * Divider:
     *
     *   N = 127
     *
     * Decoded MultiSynth values:
     *
     *   P1 = 15744
     *   P2 = 0
     *   P3 = 1
     *
     * Equivalent divider form:
     *
     *   divider = a + b/c
     *           = 127 + 0/1
     *           = 127
     *
     * MultiSynth1 is identical to MultiSynth0 because CLK0 and CLK1
     * have the same frequency.
     *
     * CLK1 is used as the zero-phase reference. The CLK0/CLK1
     * quadrature relationship comes from the PHOFF registers, not from
     * these MultiSynth divider values.
     */
    { 0x32, 0x00 },
    { 0x33, 0x01 },
    { 0x34, 0x00 },
    { 0x35, 0x3D },
    { 0x36, 0x80 },
    { 0x37, 0x00 },
    { 0x38, 0x00 },
    { 0x39, 0x00 },

    /*
     * MultiSynth2: CLK2 frequency divider.
     *
     * Output:
     *
     *   CLK2 ~= 7.040100 MHz
     *
     * Purpose:
     *
     *   CLK2 is the RF loopback carrier injected into the receiver path.
     *
     * Expected downconverted audio:
     *
     *   CLK2 - CLK0
     *   ~= 7.040100 MHz - 7.038600 MHz
     *   ~= 1.500 kHz
     *
     * Runtime tone stepping updates only this register block:
     *
     *   0x3A–0x41
     *
     * Divider:
     *
     *   divider = PLLA / CLK2
     *           ~= 893.9022 MHz / 7.040100 MHz
     *           ~= 126.972941
     *
     * Equivalent divider form:
     *
     *   divider = a + b/c
     *           = 126 + 972941/1000000
     *
     * Si5351 MultiSynth encoded values:
     *
     *   P1 = 15740
     *   P2 = 536448
     *   P3 = 1000000
     *
     * Register packing:
     *
     *   0x3A, 0x3B, upper nibble of 0x3F -> P3
     *   0x3C, 0x3D, 0x3E                  -> P1
     *   lower nibble of 0x3F, 0x40, 0x41 -> P2
     */
    { 0x3A, 0x42 },
    { 0x3B, 0x40 },
    { 0x3C, 0x00 },
    { 0x3D, 0x3D },
    { 0x3E, 0x7C },
    { 0x3F, 0xF8 },
    { 0x40, 0x2F },
    { 0x41, 0x80 },

    /*
     * Additional ClockBuilder-generated configuration registers.
     *
     * Keep these unchanged unless regenerating the complete Si5351
     * frequency plan.
     */
    { 0x5A, 0x00 },
    { 0x5B, 0x00 },

    { 0x95, 0x00 },
    { 0x96, 0x00 },
    { 0x97, 0x00 },
    { 0x98, 0x00 },
    { 0x99, 0x00 },
    { 0x9A, 0x00 },
    { 0x9B, 0x00 },

    /*
     * Phase offset registers.
     *
     * CLK0 and CLK1 are used as the mixer clock pair, so they should be
     * approximately 90 degrees apart.
     *
     * MultiSynth0 and MultiSynth1 set the CLK0/CLK1 output frequency.
     * The PHOFF registers set the relative phase offset between them.
     *
     * For this register map:
     *
     *   PLLA ~= 893.9022 MHz
     *   CLK0/CLK1 = 7.0386 MHz
     *
     * The Si5351 phase offset count is based on the PLL VCO clock.
     *
     * Approximate 90 degree offset:
     *
     *   phase_count = Fvco / Fout
     *               ~= 893.9022 MHz / 7.0386 MHz
     *               ~= 127
     *               ~= 0x7F
     *
     * The current ClockBuilder/lab value is:
     *
     *   CLK0_PHOFF = 0x83
     *   CLK1_PHOFF = 0x00
     *
     * 0x83 is decimal 131, which is close to the calculated 90 degree
     * count. So CLK0 is delayed relative to CLK1 by approximately one
     * quarter cycle.
     */
    { 0xA2, 0x00 },
    { 0xA3, 0x00 },
    { 0xA4, 0x00 },
    { 0xA5, 0x83 },   /* CLK0 phase offset */
    { 0xA6, 0x00 },   /* CLK1 phase offset */

    /*
     * Crystal/internal load configuration.
     *
     * This value comes from the ClockBuilder-generated,
     * board-verified configuration for the Si5351 on this WSPR-SDR PCB.
     *
     * Keep unchanged unless the full clock configuration is regenerated.
     */
    { 0xB7, 0x92 },
};

#endif