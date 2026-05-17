# e3607_proj

ELEC3607 Group Project — hardwired-loopback WSPR transmitter and decoder test.

This repository contains the C transmitter, Si5351 control code, WSPR encoder, decoder support files, and helper scripts used for the ELEC3607 WSPR-SDR project.

## Project summary

The project uses the unused Si5351 `CLK2` output as a WSPR RF source. The AUP-ZU3 board programs the Si5351 over I2C, generates the four WSPR tones on `CLK2`, and feeds the signal back into the WSPR-SDR receiver input through external attenuation.

The loopback signal path is:

```text
AUP-ZU3 software
→ Si5351 CLK2 WSPR RF tones
→ RF attenuators
→ WSPR-SDR receiver input
→ audio output
→ USB audio adapter
→ k9an-wsprd decoder
