#!/bin/bash

set -euo pipefail

ROOT_DIR="$(pwd)"
OUTPUT_DIR="$ROOT_DIR/include/gen"
CONFIG_DIR="$ROOT_DIR/config"
DEFAULT_CONFIG="default-riscv"
OUT_HEADER="$OUTPUT_DIR/config.h"
CONFIG_NAME="${1:-$DEFAULT_CONFIG}"

CONFIG_FILE="$CONFIG_DIR/$CONFIG_NAME"
if [ ! -f "$CONFIG_FILE" ]; then
    echo "This config does not exist, exiting" >&2
    exit 1
elif [ ! -s "$CONFIG_FILE" ]; then
    echo "Config file is empty, exiting" >&2
    exit 1
fi

if [ -z "${1:-}" ]; then
    echo "No config passed, using $DEFAULT_CONFIG"
fi

prepare() {
    mkdir -p "$OUTPUT_DIR"
    rm -f "$OUT_HEADER"
    touch "$OUT_HEADER"

    cat << 'EOF' > "$OUT_HEADER"
#ifndef KERNEL_CONFIG_H
#define KERNEL_CONFIG_H

/* Automatically generated file; DO NOT EDIT. */

EOF
}

write_header() {
    local selected_config="$1"
    local value="$2"
    local selected_config_upp="${selected_config^^}"

    if [ "$value" = "0" ] || [ "$value" = "n" ]; then
        echo "$selected_config ignored"
    elif [ "$value" = "1" ] || [ "$value" = "y" ]; then
        echo "#define __${selected_config_upp}__ " >> "$OUT_HEADER"
    else
        echo "#define __${selected_config_upp}__ ${value}" >> "$OUT_HEADER"
    fi
}
generate_seed() {
   MONTH="${month:-$(date +%-m)}"
   DAY="${day:-$(date +%-d)}"
   TIME_H="${time_h:-$(date +%-H)}"
   TIME_M="${time_m:-$(date +%-M)}"
   TIME_S="${time_s:-$(date +%-S)}"
   PACKED_TIME=$(( (MONTH << 22) | (DAY << 17) | (TIME_H << 12) | (TIME_M << 6) | TIME_S ))
   BUILD_SALT=$(od -An -N4 -tu4 /dev/urandom | tr -d ' ')
   RAW_MAGIC=$(( (BUILD_SALT << 32) | PACKED_TIME ))
   if [[ $RAW_MAGIC =~ ^-.* ]]; then
      RAW_MAGIC="${RAW_MAGIC:1}"
   fi
   echo $RAW_MAGIC
}
inspect_file() {
    local i=1
    local lines_file
    lines_file=$(wc -l < "$CONFIG_FILE")

    while IFS= read -r part || [ -n "$part" ]; do
        echo "Reading file [$i / $lines_file]"

        if [[ -z "${part// }" ]] || [[ "$part" =~ ^[[:space:]]*# ]]; then
            ((i++))
            continue
        fi

        local key="${part%%=*}"
        local val="${part#*=}"

        key="${key#"${key%%[![:space:]]*}"}"
        key="${key%"${key##*[![:space:]]}"}"
        val="${val#"${val%%[![:space:]]*}"}"
        val="${val%"${val##*[![:space:]]}"}"
        if [ $key = "RANDOM_SEED" ] &&  [ $val = 0 ]; then
           val=$(generate_seed)
        fi
        write_header "$key" "$val"
        ((i++))
    done < "$CONFIG_FILE"

    echo "" >> "$OUT_HEADER"
    echo "#endif" >> "$OUT_HEADER"
    echo "Finished"
}

prepare
inspect_file
echo "Generated $OUT_HEADER successfully."
