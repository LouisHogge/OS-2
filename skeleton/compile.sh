#!/bin/bash

gcc `pkg-config --cflags gtk4` *.c `pkg-config --libs gtk4` -lm -Wno-deprecated-declarations 2> error.log

if [ $? -eq 0 ]; then
  echo "Compilation successful."
else
  echo "Error occurred during compilation. Check error.log for details."
fi

