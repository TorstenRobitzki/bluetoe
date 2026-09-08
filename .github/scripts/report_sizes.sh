#!/bin/sh
#
# Records the code and RAM size of every example binary.
#
# usage: report_sizes.sh <arm-none-eabi-size> <build directory> <output directory>
#
# Writes <output directory>/sizes.md (a Markdown table for the GitHub job summary)
# and <output directory>/sizes.csv (for scripted comparison between builds).
#
# Flash is text + data (code, constants and the initial values of initialised variables),
# RAM is data + bss (initialised and zero-initialised variables; the stack is not included).

set -eu

if [ $# -ne 3 ]; then
    echo "usage: $0 <arm-none-eabi-size> <build directory> <output directory>" >&2
    exit 2
fi

size_tool=$1
build_dir=$2
out_dir=$3

mkdir -p "$out_dir"
md=$out_dir/sizes.md
csv=$out_dir/sizes.csv

{
    echo "## Example binary sizes"
    echo
    echo "| Example | Flash | RAM | text | data | bss |"
    echo "|---|---:|---:|---:|---:|---:|"
} > "$md"

echo "example,flash,ram,text,data,bss" > "$csv"

found=0

for elf in "$build_dir"/*.elf; do
    if [ ! -f "$elf" ]; then
        break
    fi

    found=1
    name=$(basename "$elf" .elf)

    # Berkeley format, last line: text data bss dec hex filename
    set -- $( "$size_tool" -B "$elf" | tail -n 1 )
    text=$1
    data=$2
    bss=$3
    flash=$(( text + data ))
    ram=$(( data + bss ))

    echo "| $name | $flash | $ram | $text | $data | $bss |" >> "$md"
    echo "$name,$flash,$ram,$text,$data,$bss" >> "$csv"
done

if [ "$found" -eq 0 ]; then
    echo "error: no .elf files found in $build_dir" >&2
    exit 1
fi

cat "$md"
