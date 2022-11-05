#!/bin/bash
find . -maxdepth 1 -iname "*.jpg" | xargs -L1 -I{} convert -resize 100x "{}" ../"{}"
