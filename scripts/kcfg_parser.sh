#!/bin/bash

ROOT_DIR="$(pwd)"
KCFG_LIST=$(find . -iname "*.kcfg")
TMP="$ROOT_DIR/kcfg_tmp"
DUMP="$TMP/all_kcfg"
OUTDIR="$ROOT_DIR/config/gen"
OUT="$OUTDIR/custom"

declare -a dep

prepare() {
    if [ ! -d "$OUTDIR" ]; then
        mkdir -p "$OUTDIR"
    fi
    if [ ! -d "$TMP" ]; then
        mkdir -p "$TMP"
    fi

    rm -rf "$TMP"/*

    echo "# Config file for custom build with kcfg" > "$OUT"
    printf "\n" >> "$OUT"
    echo "/* Automatically generated file, DO NOT EDIT */" >> "$OUT"

    for i in $KCFG_LIST; do
        cat "$i" >> "$DUMP"
    done
} # prepare

divide_files() {
    local var=""
    while IFS= read -r raw_line || [ -n "$raw_line" ]; do
        line=$(echo "$raw_line" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')
        
        if [[ $line == \#* ]] || [[ $line == //* ]] || [ -z "$line" ]; then
            continue
        elif [[ $line == var\ * ]]; then
            var="${line#var }"
            var="${var%:}"
            touch "$TMP/$var"
            continue
        elif [[ $line == *depends_on:* ]]; then
            if [[ $line == *depends_on:\ none* ]]; then
                printf "\n" >> "$TMP/$var"
                continue
            else
                echo "$line" | cut -d':' -f2 | xargs >> "$TMP/$var"
            fi
        elif [[ $line == *default:* ]]; then
            if [[ $line == *default:\ 0* ]]; then
                continue
            else
                echo "1" >> "$TMP/$var"
            fi
        elif [[ $line == *active:* ]]; then
            if [[ $line == *active:\ 0* ]]; then
                rm -rf "$TMP/$var"
            fi
        elif [[ $line == *value:* ]]; then
            if [[ $line == *value:\ none* ]]; then
                printf "\n" >> "$TMP/$var"
                continue
            else
                echo "$line" | cut -d':' -f2 | xargs >> "$TMP/$var"
            fi
        else
            echo "Unknown error, $raw_line malformed"
        fi
    done < "$DUMP"
} # divide_files

get_deps() {
    rm -f "$DUMP"
    dep=()
    for i in "$TMP"/*; do
        [ -e "$i" ] || continue
        local d
        d=$(sed -n '1p' "$i" | xargs)
        if [ -n "$d" ] && [ "$d" != "none" ]; then
            dep+=("$d")
        fi
    done
} # get_deps

check_deps() {
    for i in "${dep[@]}"; do
        local dep_match=0
        for idx in "$TMP"/*; do
            [ -e "$idx" ] || continue
            var_name=$(basename "$idx")
            if [ "$i" = "$var_name" ]; then
                dep_match=1
                break
            fi
        done

        if [ "$dep_match" -eq 1 ]; then
            continue
        else
            echo "Error: '$i' is needed but not activated"
            exit 1
        fi
    done
    echo "All dependencies matched"
} # check_deps

create_config() {
    for i in "$TMP"/*; do
        [ -e "$i" ] || continue
        var_name=$(basename "$i")
        upp_i="${var_name^^}"
        raw_value=$(sed -n '3p' "$i" | xargs 2>/dev/null)
        if [ -z $raw_value ]; then
          value=1
        else
          value=$(echo $value | cut -d ':' -f 2)
        fi
        echo "$upp_i=$value" >> "$OUT"
    done
    echo "Finished creating $OUT"
} # create_config

clean() {
    echo "cleaning"
    rm -rf "$TMP"
} # clean

prepare
divide_files
get_deps
check_deps
create_config
clean
