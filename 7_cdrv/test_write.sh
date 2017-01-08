#!/bin/sh

DEV=/dev/mydev0

./resize $DEV 25
exec 8> $DEV
exec 9< $DEV

echo -n "11111222223333344444555556666677777888889999900000" >&8 & # len: 50

sleep 5
./clear ${DEV}

sleep 5
head -c 25 <&9

exec 8>&-
exec 9<&-