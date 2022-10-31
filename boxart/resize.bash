#!/bin/bash
mkdir -p _100x100
find . -maxdepth 1 -iname "*.jpg" | xargs -L1 -I{} convert -resize 100x100 "{}" _100x100/"{}"
