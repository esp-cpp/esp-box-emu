#!/bin/bash
find . -maxdepth 1 -iname "*.jpg" | xargs -L1 -I{} magick -quiet "{}" -resize 100x ../"{}"
