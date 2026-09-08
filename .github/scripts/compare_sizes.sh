#!/bin/sh
#
# Compares the example binary sizes of the current build with those of a baseline build.
#
# usage: compare_sizes.sh <baseline sizes.csv> <current sizes.csv> <output markdown>
#
# Both CSV files are produced by report_sizes.sh. The result is a Markdown table with the
# change in Flash and RAM usage per example. For every example whose Flash usage grew by
# more than SIZE_WARN_BYTES (default: 256) bytes, a GitHub warning annotation is emitted.

set -eu

if [ $# -ne 3 ]; then
    echo "usage: $0 <baseline sizes.csv> <current sizes.csv> <output markdown>" >&2
    exit 2
fi

baseline=$1
current=$2
out=$3
warn_bytes=${SIZE_WARN_BYTES:-256}

awk -F, -v warn_bytes="$warn_bytes" -v out="$out" '
    function signed( d ) {
        return d > 0 ? "+" d : d ""
    }

    # first file: the baseline
    NR == FNR {
        if ( FNR > 1 )
        {
            base_flash[ $1 ] = $2
            base_ram[ $1 ]   = $3
        }
        next
    }

    # second file: the current sizes; the header line starts the table
    FNR == 1 {
        print "## Size comparison with baseline" > out
        print "" > out
        print "| Example | Flash | Flash change | RAM | RAM change |" > out
        print "|---|---:|---:|---:|---:|" > out
        next
    }

    {
        name  = $1
        flash = $2
        ram   = $3

        if ( name in base_flash )
        {
            flash_change = signed( flash - base_flash[ name ] )
            ram_change   = signed( ram - base_ram[ name ] )

            if ( flash - base_flash[ name ] > warn_bytes )
                printf "::warning::Flash usage of example %s grew by %d bytes (%d -> %d)\n", name, flash - base_flash[ name ], base_flash[ name ], flash
        }
        else
        {
            flash_change = "new"
            ram_change   = "new"
        }

        printf "| %s | %d | %s | %d | %s |\n", name, flash, flash_change, ram, ram_change > out
    }
' "$baseline" "$current"

cat "$out"
