from __future__ import print_function
import asyncore
import socket
import sys
import time
import tsdict
import peccoCommandCenter
import peccoLogger

if sys.version_info.major == 3:
    import queue as Queue
else:
    import Queue

class TcpServer(asyncore.dispatcher):
    def __init__(self, logger, configuration):
        asyncore.dispatcher.__init__(self)
        self.config = configuration
        self.name = "TcpServer"
        self.logger = peccoLogger.PadmeLoggerProxy(logger, self.name, level=True)
        self.port = int(self.config["TCPPort"])
        self.create_socket(socket.AF_INET, socket.SOCK_STREAM)
        self.set_reuse_addr()
        self.bind(("", self.port))
        self.logger.debug("TcpServer - socket created:", repr(self.socket.getsockname()))
        self.listen(5)
        self.handlerList = []

    def handle_accept(self):
        client_data = self.accept()

        if client_data is not None:
            self.logger.debug("TcpServer got connection from  %s - creating handler."%repr(client_data[1]))
            self.handlerList.append(TcpConnectionHandler(client_data[0], self.logger, self.commandQueue))

    def setCommandQueue(self, queue):
        self.commandQueue = queue

    def exit(self):
        self.sock.close()


class TcpConnectionHandler(asyncore.dispatcher_with_send):
    def __init__(self, sock, logger, commandQueue=None):
        asyncore.dispatcher_with_send.__init__(self)
        self.set_socket(sock)
        self.name = "TcpConnectionHandler-for-%s"%repr(sock.getpeername())
        self.logger = logger
        self.tokenId = 0
        self.answerQueue = Queue.Queue()
        self.commandQueue = commandQueue
        self.logger.trace(self.name)

    def setCommandQueue(self, queue):
        self.commandQueue = queue

    def handle_read(self):
        data = self.recv(8192)
        self.logger.trace("Handler received data: ", data)
        if data:
            info = data.split()
            cmd = peccoCommandCenter.Command(info, answerQueue=self.answerQueue)
            self.commandQueue.put(cmd)

    def writable(self):
        return not self.answerQueue.empty()

    def handle_write(self):
        #print("yes!!!!!!")
        try:
            data = self.answerQueue.get(timeout=0.01)
            #packet = "%s %s %s\n"%(data.receiver(), data.command(), " ".join(data.args()))
            #    print(packet)
            #print(data.receiver(), data.command(), " ".join(data.args()))
            
        except Queue.Empty:
            return

        args = " ".join(map(repr, data.args()))
        packet = "%s %s %s\n"%(data.receiver(), data.command(), args)
        self.send(packet)

    def exit(self):
        self.sock.close()
