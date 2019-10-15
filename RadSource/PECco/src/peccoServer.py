#!/usr/bin/env python2
# -*- coding: utf-8 -*-
"""
Created on Wed Sep 20 12:11:54 2017

@author: franz
"""
from __future__ import print_function
import argparse
import os
import re
import signal
import sys
import time
import asyncore

import pccServer
import pccCommandCenter
import pccLogger
import pccTcpServer
import pccMoveController
import pccHVController
import pccRCController
import pccSequencerController

# start and setup server
# check config files
# check availability of infos

# ask for list of crystals from operator
# ask for list of voltages from operator

# set voltage of HV to V0
# position XY motors for source @ 0,0
# move to position of center of first crystal
# check HV 
# if okay, start, else wait 1s
# config run, crystal mask, crystal name for output file
# start run
# wait...
# run finished? start postprocessing
# move to next crystal... again


threadList = []

def signalHandler(signum, frame):
    print('Signal handler called with signal', signum)
    print("Trying to join/exit the threads...")
    print(threadList)
    for threadIdx in range(len(threadList)-1,-1,-1):
        print(threadIdx, threadList[threadIdx].name)
        threadList[threadIdx].exit()
        threadList[threadIdx].join()

    sys.exit(0)

def parseArguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default=42424, type=int, help="the TCP port for the server")
    parser.add_argument("--config", type=str, help="the configuration file for the server")
    return parser.parse_args()


#def loadConfiguration(fname="pcc_configuration.txt"):
#    configuration = {}
#    configuration["MovementServer"] = "192.168.0.52"
#    configuration["TCPPort"] = 42424
#    configuration["DAQConfigPath"] = "./"
#    return configuration    

def loadConfiguration(fname="pcc_configuration.txt"):
    try:
         conffile = open(fname)
    except:
        print("There was a problem opening the config file %s"%fname)
        sys.exit(-1)
    info = conffile.readlines()
    conffile.close()

    data = {}
    confLine = re.compile("^\s*(\w+)\s*=\s*(.*)\s*$")
    for line in info:
        yy = confLine.match(line)
        if yy:
            k, v = yy.groups()
            data[k] = v

    print("Data: ", data) 
    return data

if __name__ == "__main__":
    
    signal.signal(signal.SIGINT, signalHandler)
    signal.signal(signal.SIGHUP, signalHandler)

    args = parseArguments()

    # this will be replaced by the configuration reader/parser
    configuration = loadConfiguration()

    # create the Logger
    theLogger = pccLogger.PadmeLogger()
    threadList.append(theLogger)
    theLogger.start()
    theLogger.addWriter("print", pccLogger.PrintLoggerObject())
    theLogger.addWriter("write2file", pccLogger.LogMessage("%s/pcc_server.log"%configuration["DAQConfigPath"]))
    theLogger.trace("logger is active...")

    # create the CommandCenter
    theCommandCenter = pccCommandCenter.CommandCenter(theLogger, configuration)
    threadList.append(theCommandCenter)
    theCommandCenter.start()

    # create the TcpServer, does not need to be start()ed as it uses asyncore for operation
    theTcpServer = pccTcpServer.TcpServer(theLogger, configuration)
    theCommandCenter.inputQueue.put(pccCommandCenter.Command(("CommandCenter", "addModule", theTcpServer.name, theTcpServer)))

    # create the MovementController
    theMoveController = pccMoveController.MoveController(theLogger, configuration)
    threadList.append(theMoveController)
    theMoveController.start()
    theCommandCenter.inputQueue.put(pccCommandCenter.Command(("CommandCenter", "addModule", theMoveController.name, theMoveController)))

    # create the HVController
    theHVController = pccHVController.HVController(theLogger, configuration)
    threadList.append(theHVController)
    theHVController.start()
    theCommandCenter.inputQueue.put(pccCommandCenter.Command(("CommandCenter", "addModule", theHVController.name, theHVController)))

    # create the RCController
    theRCController = pccRCController.RCController(theLogger, configuration)
    threadList.append(theRCController)
    theRCController.start()
    theCommandCenter.inputQueue.put(pccCommandCenter.Command(("CommandCenter", "addModule", theRCController.name, theRCController)))
    
    # create the Sequencer
    theSequencerController = pccSequencerController.SequencerController(theLogger, configuration)
    threadList.append(theSequencerController)
    theSequencerController.start()
    theCommandCenter.inputQueue.put(pccCommandCenter.Command(("CommandCenter", "addModule", theSequencerController.name, theSequencerController)))

    print("All done, ready to go...")

    asyncore.loop(timeout=0.1)
    time.sleep(1)

    for threadIdx in range(len(threadList)-1,-1,-1):
        threadList[threadIdx].exit()
        threadList[threadIdx].join()

    # theCommandCenter.exit()
    # theCommandCenter.join()
    # print("CC joined....")
    # theLogger.exit()
    # #theLogger.trace("Time to die...")
    # theLogger.join()
    print("Done!")
