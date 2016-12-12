# connection to the shell:
netcat 127.0.0.1 4315
# test command for multithreaded log write
(for((;;));do echo -e "info\n"; done;) | netcat 127.0.0.1 4315