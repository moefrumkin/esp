#!/bin/bash
./socgen/esp/esplink --regw -a 0x60090b8c -d 0x00064095
./socgen/esp/esplink --regr -a 0x60090b8c
./socgen/esp/esplink --regw -a 0x60090d8c -d 0x0007f095
./socgen/esp/esplink --regr -a 0x60090d8c
./socgen/esp/esplink --regw -a 0x6009138c -d 0x0007f095
./socgen/esp/esplink --regr -a 0x6009138c
./socgen/esp/esplink --regw -a 0x6009158c -d 0x0007f095
./socgen/esp/esplink --regr -a 0x6009158c
