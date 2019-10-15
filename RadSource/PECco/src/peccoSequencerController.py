import pccBaseModule
import pccCommandCenter
import pccDAQConfigFileMaker as dm
import copy
import os
import sys
import threading

if sys.version_info.major == 3:
    import queue as Queue
else:
    import Queue

movementPattern = [
    # Vidx, Row, Col1,Col2,..,.
    (0, (0, (0, 1, 2, 3, 4))),
    (0, (1, (4, 3, 2, 1, 0))),
    (0, (2, (0, 1, 2, 3, 4))),
    (0, (3, (4, 3, 2, 1, 0))),
    (0, (4, (0, 1, 2, 3, 4))),
    (1, (3, (4, 3, 2, 1, 0))),
    (1, (4, (0, 1, 2, 3, 4))),
    (1, (2, (4, 3, 2, 1, 0))),
    (1, (1, (0, 1, 2, 3, 4))),
    (1, (0, (4, 3, 2, 1, 0))),
    (2, (1, (0, 1, 2, 3, 4))),
    (2, (0, (4, 3, 2, 1, 0))),
    (2, (2, (0, 1, 2, 3, 4))),
    (2, (3, (4, 3, 2, 1, 0))),
    (2, (4, (0, 1, 2, 3, 4))),
    (3, (3, (4, 3, 2, 1, 0))),
    (3, (4, (0, 1, 2, 3, 4))),
    (3, (2, (4, 3, 2, 1, 0))),
    (3, (1, (0, 1, 2, 3, 4))),
    (3, (0, (4, 3, 2, 1, 0))),
    (4, (1, (0, 1, 2, 3, 4))),
    (4, (0, (4, 3, 2, 1, 0))),
    (4, (2, (0, 1, 2, 3, 4))),
    (4, (3, (4, 3, 2, 1, 0))),
    (4, (4, (0, 1, 2, 3, 4)))
]


class Sequencer(pccBaseModule.BaseModule):
    def __init__(self, logger, configuration, sequenceConfigFile):
        pccBaseModule.BaseModule.__init__(self, logger, configuration)
        self.name = "Sequencer"
        self.setupLoggerProxy()
        self.sequenceConfigFile = sequenceConfigFile

        # Load the actual test setup
        self.readInConfiguration()

        if not self.hasTestSetConfig:
            return

        # Parse the test set configuration
        self.testSetConfigParser()

        # Setup the DAQ dir based on the available info
        self.setupDAQ()

        # Prepare the test sequences
        self.createSequence()

        self.dataMutex      = threading.Lock()
        self.pauseNow       = False
        self.stopNow        = False
        self.runningNow     = False
        self.daqSkipNextRun = False
        self.HVControllerTokenID = -42424         

    def setupCmdDict(self):
        #self.cmdDict["status"] = self.status
        #self.cmdDict["getSequence"] = self.getSequence
        self.cmdDict["resetXY"] = self.resetXY
        self.cmdDict["moveXY"] = self.moveXY
        self.cmdDict["setHV"] = self.setHV
        self.cmdDict["runDAQ"] = self.runDAQ
        self.cmdDict["turnOnChannels"] = self.turnOnChannels
        self.cmdDict["turnOffChannels"] = self.turnOffChannels
        self.cmdDict["setHVtoSafe"] = self.setHVtoSafe

    def stop(self):
        self.dataMutex.acquire_lock()
        self.stopNow = True
        self.dataMutex.release_lock()

    def pause(self):
        self.dataMutex.acquire_lock()
        self.pauseNow = True
        self.dataMutex.release_lock()
        
    def resume(self):
        self.dataMutex.acquire_lock()
        self.stopNow = False
        self.dataMutex.release_lock()

    def status(self):
        self.dataMutex.acquire_lock()
        datum = (self.runningNow, self,pauseNow, self,stopNow)
        self.dataMutex.release_lock()
        return datum
        
    def readInConfiguration(self):
        self.hasTestSetConfig = False
        if not os.path.isfile(self.sequenceConfigFile):
            self.logger.warn("%s cannot read the configuration file: %s", self.name, self.sequenceConfigFile)
            return

        df = open(self.sequenceConfigFile)
        myConfig = df.readlines()
        df.close()
        
        myConfig2 = filter(lambda x: (x[0]!="#" and x.strip()!=''), myConfig)
    
        self.testSetConfig = dict(map(lambda y: (y[0].strip(), y[1].strip()), map(lambda x: x.strip().split("="), myConfig2)))
        self.hasTestSetConfig = True

        self.logger.debug(self.testSetConfig)

        self.sequenceName = self.testSetConfig["SequenceName"]

    def testSetConfigParser(self):
        if not self.hasTestSetConfig:
            self.logger.warn("%s cannot start sequence %s: no test set configuration available"%(self.name, self.sequenceName))
        
        self.crystalMatrix = []
        for x in range(5):
            # the list neeeds to be formed correctly to then store the data....
            self.crystalMatrix.append([])
            for y in range(5):
                self.crystalMatrix[x].append(("%d%d"%(x,y), "disabled", [0]))

        for pX in range(5):
            for pY in range(5):
                CrystalID = "Crystal%d%d"%(pX,pY)
                self.logger.debug(pX, pY, CrystalID)
                if CrystalID in self.testSetConfig:
                    self.logger.debug("found")
                    datum = self.testSetConfig[CrystalID]
                    crystalID, voltageSet = datum.split()

                    vs = voltageSet.split(":")
                    if len(vs) != 5:
                        vsmin, vsmax = [float(x) for x in voltageSet.split("-")]
                        delta = (vsmax-vsmin)/4.
                        vs = [vsmin, vsmin+delta, vsmin+2*delta, vsmin+3*delta, vsmax]
                    else:
                        vs = [float(x) for x in vs]

                    if len(vs) != 5:
                        self.logger.error("There's something wrong with the configuration of crystal %s voltage points: %s."%(crystalID, vs))
                        #return

                    self.crystalMatrix[pX][pY] = ("%d%d"%(pX,pY), crystalID, vs)
            
        self.logger.debug("This is the crystal matrix: ", self.crystalMatrix)

    def setupDAQ(self):
        daqPath = "%s/%s"%(self.config["DAQConfigPath"], self.sequenceName)
        dm.mkDaqConfigDir(daqPath)

        daqConfigFiles = {}        

        for crystalX in range(5):
            for crystalY in range(5):
                position = "%d%d"%(crystalX, crystalY)
                cf = dm.mkDaqConfigFile(daqPath, "CrystalTesting", position)
                daqConfigFiles[position] = cf

        self.logger.debug(daqConfigFiles)
        self.config["DAQConfigFiles"] = daqConfigFiles

    def createSequence(self):

        dataPoints = []
        for step in movementPattern:
            vIdx = step[0]
            row = step[1][0]
            for col in step[1][1]:
                dataPoints.append((vIdx, row, col, self.crystalMatrix[row][col][0], self.crystalMatrix[row][col][1], self.crystalMatrix[row][col][2]))

        self.logger.debug("Always the crystal matrix: ", self.crystalMatrix)

        self.crystalXsize = int(self.config["CrystalXSize"])
        self.crystalYsize = int(self.config["CrystalYSize"])

        # These two parameters are the position of the "zero" of 
        # the stepper motors with respect to the corner of the crystal array
        # They are necessary to calculate the absolute positions 
        # to move the radioactive source to.
        # For the "center" of crystal pXpY:
        # Xabs = (pX-0.5)*crystalXSize - initialXoffset
        # Yabs = (pY-0.5)*crystalYSize - initialYoffset
        self.initialXoffset = int(self.config["InitialXOffset"])
        self.initialYoffset = int(self.config["InitialYOffset"])

        sequenceIndex = 0
        self.theSequence = []
        self.theSequence.append(("setHVtoSafe", 0))
        self.theSequence.append(("syncState", 0))
        self.theSequence.append(("turnOnChannels", 0))
        self.theSequence.append(("syncState", 0))
        self.theSequence.append(("setHV", dataPoints[sequenceIndex]))
        self.theSequence.append(("moveXY", (0,0)))
        self.theSequence.append(("syncState", 0))

        while sequenceIndex < len(dataPoints):
            vIdx, pY, pX, position, crystalID, voltages = dataPoints[sequenceIndex]
            #self.logger.debug("Loading steps for:", sequenceIndex, dataPoints[sequenceIndex])
            if crystalID != "disabled" and voltages[vIdx] !=0.:
                xAbs = int(pX * self.crystalXsize - self.initialXoffset)
                yAbs = int(pY * self.crystalYsize - self.initialYoffset)
                self.logger.debug("Working on crystal %s @ row=%d column=%d, voltage point #%d"%(crystalID, pY, pX, vIdx))

                self.logger.debug("setHV to %dV"%voltages[vIdx])
                self.theSequence.append(("setHV", dataPoints[sequenceIndex]))

                self.logger.debug("moveXY to (%d,%d)"%(xAbs, yAbs))
                self.theSequence.append(("moveXY", (xAbs, yAbs)))

                self.theSequence.append(("syncState", 0))

                self.logger.debug("runDAQ")            
                self.theSequence.append(("runDAQ", (self.sequenceName, position, crystalID)))
                self.theSequence.append(("syncState",0))

            sequenceIndex += 1

        self.theSequence.append(("turnOffChannels", 0))
        self.theSequence.append(("syncState", 0))
        self.theSequence.append(("setHVtoSafe", 0))
        self.theSequence.append(("moveXY", (0,0)))
        self.theSequence.append(("syncState", 0))

        # here I'm simply filtering out multiple syncStates which are unnecessary
        theSequence2 = []
        syncStateLast = False
        for entry in self.theSequence:
            if entry[0] == "syncState":
                if not syncStateLast: 
                    syncStateLast = True
                    theSequence2.append(entry)
            else:
                syncStateLast = False
                theSequence2.append(entry)

        self.theSequenceStored = theSequence2
        self.theSequence = copy.copy(self.theSequenceStored)

            #for entry in self.theSequence:
            #    self.logger.debug(entry)

    def startFrom(self, vIdx, px, py):
        self.logger.debug("Sequencer trying to start from: %d @ %d,%d"%(vIdx, px, py))
        self.theSequence = copy.copy(self.theSequenceStored)
        while 1:
            if self.theSequence[0][0] == "setHV":
                    vc, xc, yc, _,_,_ = self.theSequence[0][1]
                    self.logger.debug("Current entry: %d @ %d,%d"%(vc, xc, yc))
                    self.logger.debug("%s %s %s"%(vc==vIdx, xc==px, yc==py))
                    if vc==vIdx and xc==px and yc==py:
                        break
            elif len(self.theSequence)==0:
                    self.logger.info("Nothing left in the sequence... check the input parameters!")
                    break
            self.theSequence.pop(0)

        if len(self.theSequence)>0:
                self.theSequence.insert(0, ("syncState", 0))
                self.theSequence.insert(0, ("turnOnChannels", 0))
                self.theSequence.insert(0, ("syncState", 0))
                self.theSequence.insert(0, ("setHVtoSafe", 0))
                self.start()

    def run(self):
        
        commandDict = self.cmdDict
        self.logger.debug("Sequence command queue: ", self.commandQueue)

        tokenDict = {}
        msgTokenId = 65536 # base value for tokenID

        if not self.hasTestSetConfig:
            self.logger.warn("The sequencer cannot run due to a configuration problem (no sequence loaded).")
            return "The sequencer cannot run due to a configuration problem."

        self.goOn = True
        
        self.dataMutex.acquire_lock()
        self.runningNow = True
        self.dataMutex.release_lock()

        while self.goOn:
            commandList = []
            freeState = True
            #self.waitingForSync = False
            while len(self.theSequence) > 0:
                if freeState:
                    command = self.theSequence.pop(0)
                
                    if command[0]== "syncState":
                        freeState = False
                        for cmdFull in commandList:            
                            cmd, arg = cmdFull
                            msgTokenId += 1
                            tokenDict[msgTokenId] = cmd
                            self.commandQueue.put(commandDict[cmd](msgTokenId, arg))
                        commandList = []
                    else:
                        if command[0] == "runDAQ" and self.daqSkipNextRun == True:
                            self.logger.warn("Skipping DAQ run (due to HV problems) - run data: %s"%str(command))
                            self.daqSkipNextRun = False
                        else:
                            commandList.append(command)
                else:
                    if len(tokenDict) == 0:
                        freeState = True
            
                try:
                    # this works also as a "sleep"
                    cmd = self.inputQueue.get(timeout=0.01)
                    self.logger.debug("tokenDict: ", tokenDict.keys())
                    self.logger.debug("tokenDict: ", tokenDict.values())
                    self.logger.debug("Got this back: ", cmd.command(), cmd.tokenId(), cmd.args())
                    retTokenId = int(cmd.tokenId())
                    if tokenDict.has_key(retTokenId):
                        self.logger.debug(retTokenId, "is in ", tokenDict.keys())
                        self.logger.debug(retTokenId, "is in ", tokenDict.values())
                        del(tokenDict[retTokenId])
                        self.logger.debug(retTokenId, "is in ", tokenDict.keys())
                        self.logger.debug(retTokenId, "is in ", tokenDict.values())
                        if retTokenId == self.HVControllerTokenID:
                            # this should be a tuple, with the second arg a boolean
                            self.logger.debug("cmd.args(): ", cmd.args())
                            if cmd.args()[0][1] == False:
                                self.logger.warn("The HV controller had a problem with this channel. Skipping this run.")
                                self.daqSkipNextRun = True
                            else:
                                self.daqSkipNextRun = False
                    self.logger.debug("tokenDict: ", tokenDict)

                except Queue.Empty:
                    pass
            self.logger.info("%s: sequence %s complete."%(self.name, self.sequenceName))
            self.goOn = False
            self.runningNow = False


    def resetXY(self, theTokenId, args):
        self.logger.debug("here I would normally reset the position...")
        return "Done"
        return pccCommandCenter.Command(("MoveController", "resetXY"), tokenId=theTokenId, answerQueue=self.inputQueue)

    def moveXY(self, theTokenId, args):
        return pccCommandCenter.Command(("MoveController", "move_xy", args[0], args[1]), tokenId=theTokenId, answerQueue=self.inputQueue)

    def setHV(self, theTokenId, args):
        self.HVControllerTokenID = theTokenId
        return pccCommandCenter.Command(("HVController", "setHV", args), tokenId=theTokenId, answerQueue=self.inputQueue)
        
    def runDAQ(self, theTokenId, args):
        self.logger.debug("runDAQ: ", args) # sequenceName, position, crystalID
        return pccCommandCenter.Command(("RCController", "runDAQ", args[0], args[1], args[2]), tokenId=theTokenId, answerQueue=self.inputQueue)

        self.logger.debug("runDAQ: ", args) # sequenceName, position, crystalID
        return pccCommandCenter.Command(("RCController", "runDAQ", args[0], args[1], args[2]), tokenId=theTokenId, answerQueue=self.inputQueue)

    def setHVtoSafe(self, theTokenId, args):
        self.logger.debug("Setting all Vset to safe value (0V)")
        return pccCommandCenter.Command(("HVController", "setHVtoSafe", args), tokenId=theTokenId, answerQueue=self.inputQueue)

    def turnOnChannels(self, theTokenId, args):
        self.logger.debug("Turning ON all HV channels")
        return pccCommandCenter.Command(("HVController", "doAllChanOn", args), tokenId=theTokenId, answerQueue=self.inputQueue)

    def turnOffChannels(self, theTokenId, args):
        self.logger.debug("Turning OFF all HV channels")
        return pccCommandCenter.Command(("HVController", "doAllChanOff", args), tokenId=theTokenId, answerQueue=self.inputQueue)


class SequencerController(pccBaseModule.BaseModule):
    def __init__(self, logger, configuration):
        pccBaseModule.BaseModule.__init__(self, logger, configuration)
        self.name = "SequencerController"
        self.setupLoggerProxy()
        self.theSequencer = None

    #def exit(self):
    #     self.logger.warn("This needs to do a little more than the usual...")

    def setupCmdDict(self):
        self.cmdDict["loadSequence"]       = self.loadSequence
        self.cmdDict["sequencerStart"]     = self.startSequence
        self.cmdDict["sequencerStop"]      = self.stopSequence
        self.cmdDict["sequencePause"]      = self.pauseSequence
        self.cmdDict["sequenceResume"]     = self.resumeSequence
        self.cmdDict["sequencerStatus"]    = self.sequencerStatus
        self.cmdDict["sequencerStartFrom"] = self.startSequenceFrom 
 
    def loadSequence(self, *args):
        if len(args)>0:
            fname = args[0]
        else: 
            self.logger.warn("No file name found. Please check the command...")
            return "No file name found. Please check the command..."

        if os.path.isfile(fname):
            self.logger.info("%s: creating sequence from file %s"%(self.name, fname))
            self.theSequencer = Sequencer(self.logger, self.config, fname)
            self.theSequencer.setCommandQueue(self.commandQueue)
            return "The Sequencer was correctly created."
        else:
            self.logger.warn("%s: could not find file %s"%(self.name, fname))
            return "There was a problem creating the Sequencer. File %s not found"%fname
    
    def startSequence(self):
        if self.theSequencer:
            self.theSequencer.start()
            self.logger.info("%s: starting sequencer"%self.name)
        else:
            self.logger.warn("%s: could not start sequencer"%self.name)
    
    def startSequenceFrom(self, *args):
        vIdx, px, py = [int(x) for x in args]
        if self.theSequencer:
            self.logger.info("%s: starting sequencer from position row=%d, column=%d @ voltage point=%d"%(self.name, py, px, vIdx))
            self.theSequencer.startFrom(vIdx, px, py)
        else:
            self.logger.warn("%s: could not start sequencer"%self.name)            

    def stopSequence(self):
        if self.theSequencer:
            self.theSequencer.stop()
            self.logger.info("%s: stopping sequencer"%self.name)
        else:
            self.logger.warn("%s: could not stop sequencer as it doesn't exist"%self.name)            

    def resumeSequence(self):
        if self.theSequencer:
            self.theSequencer.resume()
            self.logger.info("%s: resuming sequencer"%self.name)
        else:
            self.logger.warn("%s: could not resume sequencer as it doesn't exist"%self.name)            
        
    def pauseSequence(self):
        if self.theSequencer:
            self.theSequencer.pause()
            self.logger.info("%s: pausing sequencer"%self.name)
        else:
            self.logger.warn("%s: could not pause sequencer as it doesn't exist"%self.name)            

    def sequencerStatus(self):
        pass
