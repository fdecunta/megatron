#!/bin/sh

filename="$(grep "HISTORY_FILE" config.h | cut -d "=" -f 2 | tr -d " \";")"
dir="$(dirname "$filename")"

mkdir -p "$dir"
touch "$filename"

echo "Created" "$filename"
