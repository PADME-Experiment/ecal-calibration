#!/usr/bin/python

import socket
import sys
import os
import readline
import re
import getopt

# Enable line editing for raw_input()
readline.parse_and_bind('tab: complete')

class CalibRCClient:

    def __init__(self):

        # Get port to use for RC connection from PADME_RC_PORT or use port 10000 as default
        self.runcontrol_port = int(os.getenv('PADME_RC_PORT',"10000"))

        # Create a TCP/IP socket
        self.sock = socket.socket(socket.AF_INET,socket.SOCK_STREAM)

        # Connect the socket to the port where the server is listening
        server_address = ('localhost', self.runcontrol_port)
        print 'Connecting to RunControl server on host %s port %s' % server_address
        try:
            self.sock.connect(server_address)
        except:
            print "Error connecting to server: "+str(sys.exc_info()[0])
            return

        self.main_loop()

    def get_answer(self):

        # First get length of string
        l = ""
        for i in range(5): # Max 99999 characters
            ch = self.sock.recv(1)
            if ch:
                l += ch
            else:
                self.error_exit("Server closed connection")
        ll = int(l)

        # Then read the right amount of characters from the socket
        ans = ""
        for i in range(ll):
            ch = self.sock.recv(1)
            if ch:
                ans += ch
            else:
                self.error_exit("Server closed connection")

        return ans

    def send_command(self,cmd):

        if len(cmd)<100000:
            self.sock.sendall("%5.5d"%len(cmd)+cmd)
        else:
            self.error_exit("Command too long: cannot send")

    def ask_server(self,cmd):

        self.send_command(cmd)
        return self.get_answer()

    def error_exit(self,msg):

        print "*** FATAL ERROR ***"
        print msg
        self.sock.close()
        exit(1)

    def main_loop(self):

        while True:

            # Get message to send
            message = raw_input("SEND (q or Q to Quit): ")
            if (message == 'q' or message == 'Q'):
                print "Quit command received: exiting..."
                break

            # Send message to server and get its answer
            print "Sending %s"%message
            ans = self.ask_server(message)
            print ans

            # Handle special commands
            if message == "shutdown":
                print "Server's gone. I'll take my leave as well..."
                break

            elif message == 'new_run':
                while ans != "init_ready" and ans != "init_timeout" and ans != "init_fail":
                    ans = self.get_answer()
                    print ans

            elif message == 'abort_run' or message == 'stop_run':
                while ans != "terminate_ok" and ans != "terminate_error":
                    ans = self.get_answer()
                    print ans

            elif message == 'start_run':
                while ans != "run_started":
                    ans = self.get_answer()
                    print ans

        print "Closing socket"
        self.sock.shutdown(socket.SHUT_RDWR)
        self.sock.close()

def print_help():
    print 'CalibRCClient [-h|--help]'
    print '    Start interactive client for the CalibRunControl daemon.'
    print '    Only used for testing purposes.'

def main(argv):

    try:
        opts,args = getopt.getopt(argv,"h",["help"])
    except getopt.GetoptError:
        print_help()
        sys.exit(2)

    serverInteractive = False
    for opt,arg in opts:
        if opt == '-h' or opt == '--help':
            print_help()
            sys.exit()

    CalibRCClient()

# Execution starts here
if __name__ == "__main__":
   main(sys.argv[1:])
