# connection to the shell:
netcat 127.0.0.1 5315
# test command for multithreaded log write
for((;;));do echo -e "info\ngetlog 3\nexit\n" | netcat -q 1 127.0.0.1 5315; done;
