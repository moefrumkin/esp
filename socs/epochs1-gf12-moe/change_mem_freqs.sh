#!/bin/bash
./socgen/esp/esplink --regw -a 0x60090f8c -d 0x00068095
./socgen/esp/esplink --regr -a 0x60090f8c
./socgen/esp/esplink --regw -a 0x6009178c -d 0x00068095
./socgen/esp/esplink --regr -a 0x6009178c
./socgen/esp/esplink --regw -a 0x60091b8c -d 0x00068095
./socgen/esp/esplink --regr -a 0x60091b8c
./socgen/esp/esplink --regw -a 0x60091d8c -d 0x00068095
./socgen/esp/esplink --regr -a 0x60091d8c
