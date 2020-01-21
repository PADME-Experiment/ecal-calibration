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
import atexit

import peccoServer
import peccoCommandCenter
import peccoLogger
import peccoTcpServer
import peccoMoveController
import peccoHVController
import peccoRCController
import peccoSequencerController

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
BASE_ERROR_EXIT = 42 # random number  ;)

def signalHandler(signum, frame):
    print('Signal handler called with signal', signum)
    print("Trying to join/exit the threads...")
    print(threadList)
    # moved the thread cleanup in a separate function
    # registered with atexit    
    sys.exit(0)

# this function takes care of sending the exit signal
# to all the running threads and tries to join them
# The function *always* gets automatically executed at
# the end of the program.
@atexit.register
cleanUpThreads():
    for threadIdx in range(len(threadList)-1,-1,-1):
        print(threadIdx, threadList[threadIdx].name)
        threadList[threadIdx].exit()
        threadList[threadIdx].join()
    print("Done!")
    
def parseArguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default=42424, type=int, help="the TCP port for the server")
    parser.add_argument("--config", type=str, help="the configuration file for the server")
    return parser.parse_args()


#def loadConfiguration(fname="pecco_configuration.txt"):
#    configuration = {}
#    configuration["MovementServer"] = "192.168.0.52"
#    configuration["TCPPort"] = 42424
#    configuration["DAQConfigPath"] = "./"
#    return configuration    

def loadConfiguration(fname="pecco_configuration.txt"):
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

# check that the module initialization was successful:
# proceed if yes, stop the execution otherwise
def checkModuleStatus(module, exitValue):
    status = module.checkStatus()
    if not status[0]: # status[0] is False
        print("There was a major problem with the startup of %s module. Exiting."%module.name)
        print("The error message is: ", status[1])
        print("There might be more info in the log files.")
        sys.exit(BASE_ERROR_EXIT+exitValue)

if __name__ == "__main__":
    
    signal.signal(signal.SIGINT, signalHandler)
    signal.signal(signal.SIGHUP, signalHandler)

    args = parseArguments()

    # this will be replaced by the configuration reader/parser
    configuration = loadConfiguration()

    # create the Logger
    theLogger = peccoLogger.PadmeLogger()
    threadList.append(theLogger)
    theLogger.start()
    theLogger.addWriter("print", peccoLogger.PrintLoggerObject())
    theLogger.addWriter("write2file", peccoLogger.LogMessage("%s/pecco_server.log"%configuration["DAQConfigPath"]))
    theLogger.trace("logger is active...")

    # create the CommandCenter
    theCommandCenter = peccoCommandCenter.CommandCenter(theLogger, configuration)
    checkStatus(theCommandCenter, 1)
    threadList.append(theCommandCenter)
    theCommandCenter.start()

    # create the TcpServer, does not need to be start()ed as it uses asyncore for operation
    theTcpServer = peccoTcpServer.TcpServer(theLogger, configuration)
    checkStatus(theTcpServer, 2)

    theCommandCenter.inputQueue.put(peccoCommandCenter.Command(("CommandCenter", "addModule", theTcpServer.name, theTcpServer)))

    # create the ExpMovementController
    theMoveController = peccoExpMoveController.MoveController(theLogger, configuration)
    checkStatus(theMoveController, 2)

    threadList.append(theMoveController)
    theMoveController.start()
    theCommandCenter.inputQueue.put(peccoCommandCenter.Command(("CommandCenter", "addModule", theMoveController.name, theMoveController)))

    # create the HVController
    theHVController = peccoHVController.HVController(theLogger, configuration)
    checkStatus(theHVController, 3)

    threadList.append(theHVController)
    theHVController.start()
    theCommandCenter.inputQueue.put(peccoCommandCenter.Command(("CommandCenter", "addModule", theHVController.name, theHVController)))

    # create the RCController
    theRCController = peccoRCController.RCController(theLogger, configuration)
    checkStatus(theRCController, 4)

    threadList.append(theRCController)
    theRCController.start()
    theCommandCenter.inputQueue.put(peccoCommandCenter.Command(("CommandCenter", "addModule", theRCController.name, theRCController)))
    
    # create the Sequencer
    theSequencerController = peccoSequencerController.SequencerController(theLogger, configuration)
    checkStatus(theSequenceController, 5)

    threadList.append(theSequencerController)
    theSequencerController.start()
    theCommandCenter.inputQueue.put(peccoCommandCenter.Command(("CommandCenter", "addModule", theSequencerController.name, theSequencerController)))

    print("All done, ready to go...")

    asyncore.loop(timeout=0.1)
    time.sleep(1)

    # this part has been delegated to an atexit 
    # registered finalizer function to account 
    # for the different exit paths.
    
    #for threadIdx in range(len(threadList)-1,-1,-1):
    #    threadList[threadIdx].exit()
    #    threadList[threadIdx].join()
    #print("Done!")

    # theCommandCenter.exit()
    # theCommandCenter.join()
    # print("CC joined....")
    # theLogger.exit()
    # #theLogger.trace("Time to die...")
    # theLogger.join()
