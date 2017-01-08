#!/bin/sh

DEV=/dev/mydev0

./resize $DEV 20
exec 8> $DEV
exec 7< $DEV

echo -n 12345 >&8
head -c 10 <&7 &
echo
echo
sleep 5
echo -n 67890 >&8
echo
echo
exec 7<&-
exec 8>&-