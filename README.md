# Client_Server_FileTrasfer

Client can request one or more files to server. Have fun!

# Usage:
Build it in your source folder using:

gcc -std=gnu99 -o server server1/*.c *.c -Iserver1 -lpthread -lm 
gcc -std=gnu99 -o client client1/*.c *.c -Iclient1 -lpthread -lm
gcc -std=gnu99 -o server server2/*.c *.c -Iserver2 -lpthread -lm

Execute server as:
./server 1500 

Execute client as:
usage: <dest_address> <dest_port> <filename> [filename ... ]
