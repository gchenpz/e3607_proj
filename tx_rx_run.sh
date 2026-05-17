#!/bin/sh

#
# tx_rx_test.sh
#
# One-command WSPR loopback test.
#
# This script:
#
#   1. Accepts either:
#
#        ./tx_rx_test.sh
#
#      which uses the default message:
#
#        VK2JPG QF56 3
#
#      or:
#
#        ./tx_rx_test.sh CALLSIGN LOCATOR POWER
#
#      example:
#
#        ./tx_rx_test.sh VK2ABC QF56 10
#
#   2. Waits for the next WSPR 2-minute slot boundary.
#
#   3. Starts the transmitter in the background.
#
#   4. Starts the receiver/decoder in the foreground.
#
#   5. Repeats until Ctrl-C is pressed.
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

#
# Check command-line arguments.
#
# $# means "number of command-line arguments".
#
# Allowed:
#
#   0 arguments:
#       ./tx_rx_test.sh
#       Use default message.
#
#   3 arguments:
#       ./tx_rx_test.sh CALLSIGN LOCATOR POWER
#       Use custom message.
#
# Not allowed:
#
#   1 argument or 2 arguments.
#
# Example rejected command:
#
#   ./tx_rx_test.sh VK2ABC
#
# because locator and power are missing.
#
if [ "$#" -ne 0 ] && [ "$#" -ne 3 ]; then
    echo "Usage:"
    echo "  ./tx_rx_test.sh"
    echo "  ./tx_rx_test.sh CALLSIGN LOCATOR POWER"
    echo
    echo "Examples:"
    echo "  ./tx_rx_test.sh"
    echo "  ./tx_rx_test.sh VK2ABC QF56 10"
    exit 1
fi

#
# Message setup.
#
# ${1:-VK2JPG} means:
#
#   use argument 1 if it exists,
#   otherwise use VK2JPG.
#
# ${2:-QF56} means:
#
#   use argument 2 if it exists,
#   otherwise use QF56.
#
# ${3:-3} means:
#
#   use argument 3 if it exists,
#   otherwise use 3.
#
# So:
#
#   ./tx_rx_test.sh
#
# gives:
#
#   VK2JPG QF56 3
#
# and:
#
#   ./tx_rx_test.sh VK2ABC QF56 10
#
# gives:
#
#   VK2ABC QF56 10
#
CALLSIGN="${1:-VK2JPG}"
LOCATOR="${2:-QF56}"
POWER="${3:-3}"

#
# Cleanup function.
#
# This runs when Ctrl-C is pressed.
#
# wspr_transmit is started with sudo, so sudo is also used to kill it.
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
#
# This tells the shell:
#
#   when Ctrl-C is pressed, run cleanup().
#
trap cleanup INT

#
# Create log folders once before the loop starts.
#
# TX logs go here:
#
#   logs/tx_logs/
#
# Decoder logs and decoder output files go here:
#
#   logs/decoder_logs/
#
mkdir -p logs/tx_logs logs/decoder_logs

#
# Ask for sudo password before the timing-critical part.
#
# sudo -v refreshes sudo permission.
#
# This helps avoid the transmitter being delayed by a password prompt exactly
# when the WSPR slot starts.
#
sudo -v || exit 1

#
# Main repeat loop.
#
# This keeps running one WSPR slot after another until Ctrl-C.
#
while true
do
    #
    # Refresh sudo before waiting for the next slot.
    #
    # This prevents sudo from expiring while we are waiting.
    #
    sudo -v || exit 1

    #
    # WSPR transmissions start on even 2-minute boundaries.
    #
    # date +%s gives current Unix time in seconds.
    #
    # 120 seconds = 2 minutes.
    #
    # This calculation finds the next 120-second boundary:
    #
    #   now / 120
    #       current 2-minute slot number
    #
    #   + 1
    #       next slot number
    #
    #   * 120
    #       Unix time of next slot start
    #
    now=$(date +%s)
    next=$(( ((now / 120) + 1) * 120 ))
    wait_time=$(( next - now ))

    #
    # Prepare a fresh TX log filename for this slot.
    #
    # Example:
    #
    #   logs/tx_logs/tx_20260517_041500.log
    #
    logfile="logs/tx_logs/tx_$(date +%Y%m%d_%H%M%S).log"

    #
    # Wait until the exact next WSPR slot.
    #
    echo "Waiting $wait_time seconds for next WSPR slot..."
    sleep "$wait_time"

    #
    # Print what will happen.
    #
    echo "Starting TX/RX at $(date)"
    echo "Message: $CALLSIGN $LOCATOR $POWER"
    echo "TX log: $logfile"

    #
    # Start transmitter in the background.
    #
    # The & means:
    #
    #   start wspr_transmit,
    #   keep it running in the background,
    #   immediately continue to the decoder command.
    #
    # stdbuf -oL -eL makes the transmitter log line-buffered.
    #
    # This helps the log file update while the program is running.
    #
    # The transmitter receives the chosen message here:
    #
    #   "$CALLSIGN" "$LOCATOR" "$POWER"
    #
    # So if you ran:
    #
    #   ./tx_rx_test.sh VK2ABC QF56 10
    #
    # this line becomes effectively:
    #
    #   sudo ./wspr_transmit VK2ABC QF56 10
    #
    sudo stdbuf -oL -eL ./wspr_transmit "$CALLSIGN" "$LOCATOR" "$POWER" > "$logfile" 2>&1 &
    tx_pid=$!

    #
    # Start receiver/decoder in the foreground.
    #
    # The terminal will show decoder output directly.
    #
    # The subshell:
    #
    #   (
    #       cd logs/decoder_logs || exit 1
    #       ../../pa-wsprcan/k9an-wsprd
    #   )
    #
    # means:
    #
    #   temporarily move into logs/decoder_logs,
    #   run the decoder there,
    #   then return to the original folder afterward.
    #
    # This keeps decoder files such as ALL_WSPR.TXT and hashtable.txt out of
    # the main project folder.
    #
    (
        cd logs/decoder_logs || exit 1
        ../../pa-wsprcan/k9an-wsprd
    )

    #
    # Wait until the transmitter has finished before starting the next slot.
    #
    # This avoids overlapping two transmitters.
    #
    wait "$tx_pid" 2>/dev/null

    echo "Slot finished at $(date)"
    echo
done