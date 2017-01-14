#!/bin/sh
./truncate /dev/mydev0 1
echo "AAA" > /dev/mydev0
echo "BBB" >> /dev/mydev0
echo "CCC" > /dev/mydev0
cat /dev/mydev0
./truncate /dev/mydev0 4
cat /dev/mydev0
./truncate /dev/mydev0 8
./truncate /dev/mydev0 -1
./truncate /dev/mydev0 99999
