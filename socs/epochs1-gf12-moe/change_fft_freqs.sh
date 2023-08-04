#!/bin/bash
./socgen/esp/esplink --regw -a 0x6009098c -d 0x00008095
./socgen/esp/esplink --regr -a 0x6009098c
./socgen/esp/esplink --regw -a 0x6009118c -d 0x00008095
./socgen/esp/esplink --regr -a 0x6009118c
./socgen/esp/esplink --regw -a 0x6009198c -d 0x00008095
./socgen/esp/esplink --regr -a 0x6009198c
