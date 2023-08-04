#!/bin/bash
./socgen/esp/esplink --regw -a 0x60090b98 -d 0x00000001
./socgen/esp/esplink --regr -a 0x60090b98
./socgen/esp/esplink --regw -a 0x60090d98 -d 0x00000003
./socgen/esp/esplink --regr -a 0x60090d98
./socgen/esp/esplink --regw -a 0x60091398 -d 0x00000005
./socgen/esp/esplink --regr -a 0x60091398
./socgen/esp/esplink --regw -a 0x60091598 -d 0x00000007
./socgen/esp/esplink --regr -a 0x60091598
