# follow kernel output
dmesg -w
# build
make
# clean
make clean
# install
make insmod
# remove
make rmmod
# test binaries
make test
# watch ptoc
watch -n1 cat /proc/is141315/info