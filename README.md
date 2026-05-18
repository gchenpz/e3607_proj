# WSPR Transceiver

ELEC3607 Group Project — hardwired-loopback WSPR transmitter and decoder test.

This repository contains the C transmitter, Si5351 control code, WSPR encoder, decoder support files, and helper scripts used for the ELEC3607 WSPR-SDR project.

## Project summary

The project uses the unused Si5351 `CLK2` output as a WSPR RF source. The AUP-ZU3 board programs the Si5351 over I2C, generates the four WSPR tones on `CLK2`, and feeds the RF signal back into the WSPR-SDR receiver input through external attenuation.

Signal path:

```text
AUP-ZU3 software
-> Si5351 CLK2 WSPR RF tones
-> RF attenuators
-> WSPR-SDR receiver input
-> audio output
-> USB audio adapter
-> k9an-wsprd decoder
```

This is a hardwired RF loopback test. It is not intended for over-the-air transmission.

## Repository layout

```text
include/        Header files for Si5351 and WSPR encoder
src/            Main C source files
pa-wsprcan/     WSPR decoder program
scripts/        Helper scripts
test/           Test files
tx_rx_run.sh    One-command transmit/receive loopback runner
```

Important source files:

```text
src/wspr_transmit.c      Main WSPR transmit program
src/wspr_encoder.c       WSPR message encoder
src/si5351_wspr.c        Si5351 I2C setup and CLK2 frequency update code

include/wspr_encoder.h
include/si5351_wspr.h
include/Si5351A-RevB-Registers.h
```

## Build

On the AUP-ZU3 Linux system, build the transmitter with:

```bash
gcc -Wall -Wextra -O2 \
    -Iinclude \
    src/wspr_transmit.c src/wspr_encoder.c src/si5351_wspr.c \
    -o wspr_transmit \
    -li2c
```

Make the runner script executable:

```bash
chmod +x tx_rx_run.sh
```

## Run transmitter only

```bash
sudo ./wspr_transmit CALLSIGN LOCATOR POWER
```

Example:

```bash
sudo ./wspr_transmit VK2JPG QF56 3
```

The transmitter encodes the message into 162 WSPR symbols and steps `CLK2` through the four WSPR tones.

Frequency plan:

```text
Receiver mixer clocks:  CLK0 = CLK1 = 7.038600 MHz
WSPR CLK2 tone 0:       7.040100000 MHz
Tone spacing:           12000 / 8192 = 1.46484375 Hz
Symbol duration:        about 0.6827 s
Number of symbols:      162
```

The `CLK2` transmit signal is 1.500 kHz above the 7.038600 MHz receiver mixer clock, so after mixing it appears in the audio passband for decoding.

## Run full TX/RX loopback test

Default message:

```bash
./tx_rx_run.sh
```

This sends:

```text
VK2JPG QF56 3
```

Custom message:

```bash
./tx_rx_run.sh CALLSIGN LOCATOR POWER
```

Example:

```bash
./tx_rx_run.sh VK2ABC QF56 10
```

The script:

```text
1. Accepts either no arguments or CALLSIGN LOCATOR POWER.
2. Waits for the next 120-second WSPR slot boundary.
3. Starts sudo ./wspr_transmit in the background.
4. Runs pa-wsprcan/k9an-wsprd in the foreground.
5. Saves transmitter logs under logs/tx_logs.
6. Saves decoder output under logs/decoder_logs.
7. Repeats until Ctrl-C.
```

## Hardware setup

Required hardware:

```text
AUP-ZU3 board
WSPR-SDR PCB
USB audio adapter
BNC/alligator RF loopback cable
Two 20 dB inline RF attenuators
Shared board ground connection
```

Hardwired loopback path:

```text
Si5351 CLK2
-> purple flywire
-> BNC/alligator cable
-> 40 dB RF attenuation
-> U6 SMA receiver input / BPFIN
-> receiver audio output
-> USB audio adapter
-> decoder
```

Always connect the ground clip before applying the RF loopback signal.

## Notes

- `CLK0` and `CLK1` remain fixed as receiver mixer clocks.
- Runtime WSPR modulation updates only `CLK2`.
- The RF loopback must be attenuated before entering the receiver input.
- This setup is for laboratory hardwired-loopback testing only.

## License

GPL-3.0.
