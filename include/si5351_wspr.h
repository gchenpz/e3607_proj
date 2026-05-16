#ifndef SI5351_WSPR_H
#define SI5351_WSPR_H

/*
 * Initialize the Si5351 WSPR clock infrastructure.
 *
 * This configures the fixed clock framework:
 *
 *   CLK0 = 7.0386 MHz
 *   CLK1 = 7.0386 MHz
 *   CLK2 = 7.0401 MHz initial base carrier
 *
 * After this, runtime code should only adjust CLK2.
 */
void si5351_init_wspr(void);

/*
 * Set CLK2 directly by RF frequency in Hz.
 *
 * This is the low-level runtime frequency interface.
 *
 * Example:
 *
 *   si5351_set_clk2_freq_hz(7040100.0);
 *
 * This updates only CLK2 MultiSynth2 registers:
 *
 *   0x3A–0x41
 *
 * CLK0 and CLK1 are not changed.
 */
void si5351_set_clk2_freq_hz(double clk2_hz);

/*
 * Set CLK2 using a tone number.
 *
 * This is the main interface for the baby-step test and later WSPR transmit.
 *
 * Mixer relationship:
 *
 *   audio_hz = CLK2 - MIXER_CLK_HZ
 *
 * Therefore:
 *
 *   CLK2 = MIXER_CLK_HZ + audio_hz
 *
 * Tone calculation:
 *
 *   audio_hz = base_audio_hz + tone * spacing_hz
 *
 * Baby-step example:
 *
 *   base_audio_hz = 1500.0
 *   spacing_hz    = 100.0
 *
 * gives:
 *
 *   tone 0 -> 1500 Hz audio
 *   tone 1 -> 1600 Hz audio
 *   tone 2 -> 1700 Hz audio
 *   tone 3 -> 1800 Hz audio
 *
 * Later WSPR:
 *
 *   spacing_hz = 1.46484375
 */
void si5351_set_clk2_tone(int tone, double base_audio_hz, double spacing_hz);

/*
 * Close the I2C device.
 */
void si5351_close(void);

#endif