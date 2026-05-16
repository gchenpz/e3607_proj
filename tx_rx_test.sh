#!/bin/sh

#
# tx_rx_test.sh
#
# One-command WSPR loopback test.
#
# This script:
#   1. Waits for the next WSPR 2-minute slot boundary
#   2. Starts the transmitter in the background
#   3. Starts the receiver/decoder in the foreground
#   4. Repeats until Ctrl-C is pressed
#
# Current test message:
#
#   VK2JPG QF56 3
#
# Hardware path:
#
#   Si5351 CLK2
#     -> RF attenuators
#     -> WSPR-SDR receiver input
#     -> audio output
#     -> USB audio adapter
#     -> k9an-wsprd decoder
#

CALLSIGN="VK2JPG"
LOCATOR="QF56"
POWER="3"

#
# Cleanup function.
#
# This runs when we press Ctrl-C.
#
# wspr_transmit is started with sudo, so we also use sudo to kill it.
# k9an-wsprd is started normally, so normal pkill is enough.
#
cleanup()
{
    echo
    echo "Stopping TX/RX..."

    sudo pkill -f wspr_transmit
    pkill -f k9an-wsprd

    echo "Stopped."
    exit 0
}

#
# Ctrl-C sends the INT signal.
# This tells the shell to run cleanup() when Ctrl-C is pressed.
#
trap cleanup INT

#
# Ask for sudo password before the timing-critical part.
#
# sudo -v refreshes sudo permission.
# It does not make later commands automatically sudo,
# so we still write sudo before wspr_transmit and pkill.
#
sudo -v || exit 1

while true
do
    #
    # Refresh sudo before waiting for the next slot.
    # This prevents sudo asking for a password exactly when TX should start.
    #
    sudo -v || exit 1

    #
    # WSPR transmissions start on even 2-minute UTC boundaries.
    #
    # date +%s gives current Unix time in seconds.
    # 120 seconds = 2 minutes.
    #
    # This calculation finds the next 120-second boundary.
    #
    now=$(date +%s)
    next=$(( ((now / 120) + 1) * 120 ))
    wait_time=$(( next - now ))

    echo "Waiting $wait_time seconds for next WSPR slot..."
    sleep "$wait_time"

    echo "Starting TX/RX at $(date)"
    echo "Message: $CALLSIGN $LOCATOR $POWER"

    mkdir -p logs/tx_logs logs/decoder_logs

    #
    # Start transmitter in the background.
    #
    # The & means:
    #   start wspr_transmit
    #   keep it running
    #   immediately continue to the decoder command
    #
    # TX output is saved to tx.log so the terminal is not flooded
    # with 162 symbol print lines.
    #
    logfile="logs/tx_logs/tx_$(date +%Y%m%d_%H%M%S).log"
    echo "TX log: $logfile"
    # sudo ./wspr_transmit "$CALLSIGN" "$LOCATOR" "$POWER" > logs/tx_logs/tx.log 2>&1 &
    sudo stdbuf -oL -eL ./wspr_transmit "$CALLSIGN" "$LOCATOR" "$POWER" > "$logfile" 2>&1 &
    tx_pid=$!

    #
    # Start receiver/decoder in the foreground.
    #
    # This means the terminal will show decoder output directly.
    #
    # This keeps ALL_WSPR.TXT, hashtable.txt, etc. out of the main project folder.
    #
    (
        cd logs/decoder_logs || exit 1
        ../../pa-wsprcan/k9an-wsprd
    )

    #
    # Wait until the transmitter has also finished before starting
    # the next 2-minute slot.
    #
    wait "$tx_pid" 2>/dev/null

    echo "Slot finished at $(date)"
    echo
done