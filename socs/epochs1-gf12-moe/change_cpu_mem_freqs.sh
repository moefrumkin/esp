#!/bin/bash
./socgen/esp/esplink --regw -a 0x60090b8c -d 0x000640fd
./socgen/esp/esplink --regr -a 0x60090b8c
./socgen/esp/esplink --regw -a 0x60090d8c -d 0x000640fd
./socgen/esp/esplink --regr -a 0x60090d8c
./socgen/esp/esplink --regw -a 0x6009138c -d 0x000640fd
./socgen/esp/esplink --regr -a 0x6009138c
./socgen/esp/esplink --regw -a 0x6009158c -d 0x000640fd
./socgen/esp/esplink --regr -a 0x6009158c
./socgen/esp/esplink --regw -a 0x60090f8c -d 0x000640fd
./socgen/esp/esplink --regr -a 0x60090f8c
./socgen/esp/esplink --regw -a 0x6009178c -d 0x000640fd
./socgen/esp/esplink --regr -a 0x6009178c
./socgen/esp/esplink --regw -a 0x60091b8c -d 0x000640fd
./socgen/esp/esplink --regr -a 0x60091b8c
./socgen/esp/esplink --regw -a 0x60091d8c -d 0x000640fd
./socgen/esp/esplink --regr -a 0x60091d8c
