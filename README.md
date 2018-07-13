# Client_Server_FileTrasfer

Client can request one or more files to server. Have fun!

# Usage:
Build it in your source folder using: <br>

gcc -std=gnu99 -o server server1/*.c *.c -Iserver1 -lpthread -lm <br>
gcc -std=gnu99 -o client client1/*.c *.c -Iclient1 -lpthread -lm <br>
gcc -std=gnu99 -o server server2/*.c *.c -Iserver2 -lpthread -lm <br>

Execute server as: <br>
./server 1500 

Execute client as: <br>
usage: <dest_address> <dest_port> <filename> [filename ... ]
