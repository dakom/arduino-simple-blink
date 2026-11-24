#!/bin/sh
# Print the serial port of the one Arduino Uno plugged in, or explain why not and fail.
#
# Usage: scripts/find-uno-port.sh [port]
#   With a port, just print it back (for clones, or to pick one of several boards).
#
# The port name changes between USB sockets, so look the Uno up by its USB
# vendor:product ID instead. macOS only: it uses ioreg.

if [ -n "$1" ]; then
    echo "$1"
    exit 0
fi

# ioreg prints each USB device's IDs (in decimal) just above its serial port:
#   9025:67 = 2341:0043, 9025:579 = 2341:0243, 9025:1 = 2341:0001, 10755:67 = 2a03:0043
ports=$(ioreg -r -c IOUSBHostDevice -l -w0 | LC_ALL=C awk '
    /"idVendor" = /  { vendor = $NF }
    /"idProduct" = / { product = $NF }
    /"IOCalloutDevice" = / {
        id = vendor ":" product
        if (id ~ /^(9025:(67|579|1)|10755:67)$/) { gsub(/"/, "", $NF); print $NF }
    }' | sort -u)

case $(echo "$ports" | grep -c .) in
    1)
        echo "$ports"
        ;;
    0)
        echo "No Uno found. Check that it is plugged in with a data (not charge-only) cable." >&2
        echo "For a clone, find its port with 'ls /dev/cu.*', then pass UNO_PORT=<port>" >&2
        exit 1
        ;;
    *)
        echo "More than one Uno found:" >&2
        echo "$ports" >&2
        echo "Choose one by passing UNO_PORT=<port>" >&2
        exit 1
        ;;
esac
