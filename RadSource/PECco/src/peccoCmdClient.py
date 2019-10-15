#!/usr/bin/env python

from __future__ import print_function

import socket
import sys
import os
import signal

def alarm_handler(signum, frame):
    print("No data came within timeout interval... exiting.")
    sys.exit(1)

#config_file_name = "pecco_configuration.txt"
#config_file = open(config_file_name)
#data = config.file.readlines()
#config_file.close()

#dataDict = dict([x.split("=")])

if len(sys.argv) < 2:
    print("Please put the command you wish to send on the command line.")
    sys.exit(-1)

services = {
    "cc": "CommandCenter",
    "mc": "MoveController",
    "hv": "HVController",
    "seq": "SequencerController",
    "rc": "RCController"
}

TCP_IP = '127.0.0.1'
TCP_PORT = 42424
BUFFER_SIZE = 1024

dest = sys.argv[1]
if dest in services.keys():
    dest = services[dest]


# cheap and junky technique to get out after a while...
signal.signal(signal.SIGALRM, alarm_handler)
signal.alarm(30) # 30s... it should be enough

MESSAGE = "%s %s\n"%(dest, " ".join(sys.argv[2:]))

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((TCP_IP, TCP_PORT))
s.send(MESSAGE)

fullData = []

keepGoing = True

while keepGoing:
    data = s.recv(BUFFER_SIZE)
    print(len(data), data)
    if data[-1:] == "\n":
        keepGoing = False

signal.alarm(0) # disable the alarm, just in case it fires at the wrong time...        
s.close()

print(data)
