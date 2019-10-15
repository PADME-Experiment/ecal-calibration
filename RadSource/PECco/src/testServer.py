#!/usr/bin/env python

from __future__ import print_function
import socket

TCP_IP = '127.0.0.1'
TCP_PORT = 42424
BUFFER_SIZE = 2  # Normally 1024, but we want fast response

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

s.bind((TCP_IP, TCP_PORT))
s.listen(1)

while 1:
    
    conn, addr = s.accept()
    print('Connection address:', addr)

    data2sendback = ""
    keepGoing = True

    while keepGoing:
        data = conn.recv(BUFFER_SIZE)
        if not data: 
            keepGoing = False
            break
        
        print("received data:", data)
        data2sendback += data

        if data2sendback[-1] == "\n":
            #data2sendback += "\n"
            keepGoing = False

    conn.send(data2sendback)  # echo
    conn.close()

print("And now... close the connection!")
s.close()