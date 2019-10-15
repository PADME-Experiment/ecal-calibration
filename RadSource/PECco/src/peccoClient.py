#!/usr/bin/env python2
# encoding: utf-8

from __future__ import print_function
import socket
import asyncore
import sys
import cmd
import readline
import threading
import time

if sys.version_info.major == 3:
    import queue as Queue
else:
    import Queue

class pccClient(asyncore.dispatcher):
    def __init__(self, address, port):
        asyncore.dispatcher.__init__(self)
        self.address = address
        self.port = port
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            sock.connect((self.address, self.port))
        except Exception as exc:
            print("Sorry: there was a problem connecting to server: ", exc)
            raise exc 
            
        self.set_socket(sock)

        #print(self.connect((self.address, self.port)))
        self.communicationQueue = Queue.Queue()

    def handle_connect(self):
        pass

    def handle_close(self):
        self.close()

    def handle_read(self):
        print("\n"+self.recv(8192))

    def writable(self):
        return not self.communicationQueue.empty()

    def handle_write(self):
        try:
            data = self.communicationQueue.get(timeout=0.01)
        except Queue.Empty:
            return
        #print("Sending data: ", data)

        sent = len(data)
        while sent>0:
            sent = self.send(data)
            data = data[sent:]
    
    def send_msg(self, *args):
        self.communicationQueue.put(" ".join(args))

class muThread(threading.Thread):
    def run(self):
        self.goOn = True
        while self.goOn:
            asyncore.loop(count=1, timeout=0.1)

    def exit(self):
        self.goOn = False


class CmdLine(cmd.Cmd):
    """Command line processor for pccClient."""
    def __init__(self, communicationQueue, clientThread):
        cmd.Cmd.__init__(self)
        self.communicationQueue = communicationQueue
        self.clientThread = clientThread
        self.prompt = "[pccShell]% "

    def do_exit(self, line):
        clientThread.exit()
        clientThread.join()
        sys.exit(0)

    def do_cc(self, line):
        "Send commands to the CommandCenter"
        self.communicationQueue.put("CommandCenter %s"%line)

    def do_mc(self, line):
        "Send commands to the MoveController"
        self.communicationQueue.put("MoveController %s"%line)
       
    def do_hv(self, line):
        "Send commands to the HVController"
        self.communicationQueue.put("HVController %s"%line)
       
    def do_seq(self, line):
        "Send commands to the SequencerController"
        self.communicationQueue.put("SequencerController %s"%line)

    def do_rc(self, line):
        "Send commands to the RCController"
        self.communicationQueue.put("RCController %s"%line)
 

    def do_up(self, line):
        self.communicationQueue.put("MoveController set_yrel %s"%line)

    def do_down(self, line):
        self.communicationQueue.put("MoveController set_yrel -%s"%line)

    def do_left(self, line):
        self.communicationQueue.put("MoveController set_xrel -%s"%line)

    def do_right(self, line):
        self.communicationQueue.put("MoveController set_xrel %s"%line)

    def default(self, line):
        #print("Called default: ", line)
        self.communicationQueue.put(line)
        #print(self.communicationQueue.empty())
    
    def emptyline(self):
        pass
    
if __name__ == '__main__':
    goOn = True

    # THIS HAS TO READ THE CONFIGURATION!!!!!
    client = pccClient("127.0.0.1", 42424)
    clientThread = muThread()
    clientThread.start()
    CmdLine(client.communicationQueue, clientThread).cmdloop()
