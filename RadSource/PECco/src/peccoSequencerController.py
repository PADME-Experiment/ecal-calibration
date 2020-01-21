import peccoBaseModule
import peccoCommandCenter
import peccoDAQConfigFileMaker as dm
import copy
import os
import sys
import threading

if sys.version_info.major == 3:
    import queue as Queue
else:
    import Queue

class Sequencer(peccoBaseModule.BaseModule):
    def __init__(self, logger, configuration, sequenceConfigFile):
        peccoBaseModule.BaseModule.__init__(self, logger, configuration)
        self.name = "Sequencer"
        self.setupLoggerProxy()
        
        # configuration items from general config
        self.ecalStructureFile = self.config["EcalStructureFile"]
        self.sequenceConfigFile = sequenceConfigFile
        
        # format: SequenceName Xpos Ypos Voltage
        self.lastCrystalDoneFile = "lastCrystalDone.txt"

        # Load the full ECAL Map
        self.readInECALMap()
        
        if not self.moduleStatus:
            return 

        # Load the actual test setup
        self.readInSequence()

        if not self.hasSequenceConfig:
            msg = "Missing sequence configuration file: %s"%self.sequenceConfigFile
            self.logger.warn(msg)
            self.moduleStatus = False
            self.moduleErrorMessage = msg
            return

        # Parse the sequence into a map of crystals
        self.sequenceConfigParser()
        
        # try to load the last known position to restart from there
        self.loadLastCrystalDone()
        
        if not self.moduleStatus:
            return

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
        
    def readInSequence(self):
        self.hasSequenceConfig = False
        if not os.path.isfile(self.sequenceConfigFile):
            self.logger.warn("%s cannot read the configuration file: %s", self.name, self.sequenceConfigFile)
            return

        df = open(self.sequenceConfigFile) 
        myConfig = df.readlines()
        df.close()

        # strip comments and empty lines        
        myConfig2 = filter(lambda x: (x[0]!="#" and x.strip()!=''), myConfig)

        self.sequenceConfig = {}
        self.sequenceName = (myConfig2[0].strip().split(" "))[1]
        
        for entry in myConfig2[1:]:
            info = entry.strip().split(":")
            if len(info) == 2:
                self.sequenceConfig[info[0]] = info[1]
            else: 
                self.sequenceConfig[info[0]] = "default"    

        self.hasSequenceConfig = True
        self.logger.debug(self.sequenceConfig)

    def readInECALMap(self):
        self.ecalMap = {}
        skipFirstLine = True
        try:
            with open(self.ecalStructureFile) as inFile:
                # skip first line
                _ = inFile.readline()
                for entry in inFile:
                    entry = entry.strip()
                    info = entry.split(";")
                    key = (int(info[1]), int(info[2]))
                    values = [info[0]]
                    values.extend(info[3:])
                    self.ecalMap[key] = values
        except FileNotFoundError as ff:
            msg = "ECal crystal configuration file not found: %s"%ff
            self.logger.warn(msg)
            self.moduleStatus = False
            self.moduleErrorMessage = msg
        except Exception as exc:
            msg = "Major problem in SequenceController: %s"%exc
            self.logger.warn(msg)
            self.moduleStatus = False
            self.moduleErrorMessage = msg


    def sequenceConfigParser(self):
        self.sequenceMap = {}
        
        singleCrystal  = re.compile("^C\(\s*(\d+)\s*,\s*(\d+)\s*\)$")
        xRangeCrystal  = re.compile("^C\(\s*(\d+)\s*-\s*(\d+)\s*,\s*(\d+)\s*\)$")
        yRangeCrystal  = re.compile("^C\(\s*(\d+)\s*.\s*(\d+)\s*-\s*(\d+)\s*\)$")
        xyRangeCrystal = re.compile("^C\(\s*(\d+)\s*-\s*(\d+)\s*,\s*(\d+)\s*-\s*(\d+)\s*\)\s*$")
        
        for (crystal,voltage) in self.sequenceConfig.items():
            
            # C(x,y)
            mo = singleCrystal.match(crystal)
            if mo:
                cx,cy = mo.groups()
                self.sequenceMap( (int(cx), int(cy)) ) = voltage
                continue
           
            # C(x1-x2, y)
            mo = xRangeCrystal.match(crystal)
            if mo:
                cx_min, cx_max ,cy = mo.groups()
                for xC in range(cx_min,cx_max+1):
                    self.sequenceMap(  (int(xC), int(cy)) ) = voltage
                continue
           
            # C(x, y1-y2)
            mo = yRangeCrystal.match(crystal)
            if mo:
                cx, cy_min, cy_max = mo.groups()
                for yC in range(cy_min,cy_max+1):
                    self.sequenceMap( (int(cx), int(yC)) ) = voltage
                continue

            # C(x1-x2, y1-y2)
            mo = xyRangeCrystal.match(crystal)
            if mo:
                cx_min, cx_max, cy_min, cy_max = mo.groups()
                for xC in range(cx_min,cx_max+1):
                    for yC in range(cy_min,cy_max+1):
                        self.sequenceMap( (int(xC), int(yC)) ) = voltage
                continue
               
            # program should never get here. this is parsing error!
            msg = "Crystal identifier format not recognized: %s"%crystal
            self.logger.warn(msg)
            self.moduleStatus = False
            self.moduleErrorMessage = msg
            return
    
    # this I will probably move within the actual running, lest
    # I start creating hundreds of folders that might not get used...
    # Also this has to be rewritten to work on the single crystal.        
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
        # regexes for voltage patterns:
        
        # match a number in the format x or x.y, positive or negative
        pn = "\d+|\d+.\d*"
        nn = "-\d+|-\d+.\d*"
        
        Vb = "^\s*V\s*\(\s*"
        Ve = "\s*\)\s*$"
        
        # Value: V(voltageValue)
        vValue = re.compile("%s(%s)%s"%(Vb, pn, Ve))
        
        # DeltaV: DeltaV(-deltaV1, deltaV2, #steps)
        vDelta = re.compile("%sDeltaV\(\s*(%s)\s*,\s*(%s)\s*,\s*(\d+)\s*\)%s"%(Vb, nn, pn, Ve))
        
        # RangeV: RangeV( rangeBegin, rangeEnd, #steps )
        vRange = re.compile("%sRangeV\(\s*(%s)\s*,\s*(%s)\s*,\s*(\d+)\s*\)%s"%(Vb, pn, pn, Ve))

        # SetV: V(SetV(V1, V2, ...))
        vSet = re.compile("%s(SetV\(\s*(.*)\s*\)%s"%(Vb, Ve))

        self.theSequence = []
        self.theSequence.append(("setHVtoSafe", 0))
        self.theSequence.append(("syncState", 0))
        self.theSequence.append(("turnOnChannels", 0))
        self.theSequence.append(("syncState", 0))

        # crystals are "numbered" 0 <-> 28 in X, 0<->28 in Y 
        xstart, xend, xstep = 28, -1, -1 
        for theY in range(0, 29):
            # move back and forth on the X axis => move right on line Y1, move left on line Y1+1 and so on
            if xstart == 28:
                xstart, xend, xstep = 0, 29, 1
            else:
                xstart, xend, xstep = 28, -1, -1 
                 
            for theX in range(xstart, xend, xstep):
                theCrystal = (theX, theY)
                crystalID = "X%02dY%02d"%(theX,theY)
                
                # the ecalMap is the ultimate authority as to where the crystal are
                if theCrystal not in self.ecalMap:
                    continue
                
                # extract the default information about the current crystal    
                crystalData = self.ecalMap[theCrystal]
                SlotHV, ChHV, SlotDAQ, ChDAQ, defaultHV = crystalData[1:6]
                
                # is the crystal in the loaded sequence?
                if theCrystal not in self.sequenceMap:
                    continue

                # move to the right position, the syncState relative to this is added
                # with the HV control further below
                self.logger.debug("moveXY to (%d,%d)"%(theX, theY))
                self.theSequence.append(("moveXY", theCrystal))
                
                voltageList = []
                
                crystalVoltage = self.sequenceMap[theCrystal]
                if crystalVoltage == "default":
                    voltageList = [defaultHV]
                    
                mo = vValue.match(crystalVoltage)
                if mo:
                    voltage = float(mo.groups()[0])
                    voltageList = [voltage]

                mo = vDelta.match(crystalVoltage)
                if mo:
                    md, pd, steps = [float(x) for x in mo.groups()]
                    delta = (pd-md)/steps
                    for stp in range(int(steps+1)):
                        voltageList.append(defaultHV+md+delta*stp)

                mo = vRange.match(crystalVoltage)
                if mo:
                    vMin, vMax, steps = [float(x) for x in mo.groups()]
                    delta = (vMax-vMin)/steps
                    for stp in range(int(steps+1)):
                        voltageList.append(vMin+stp*delta)

                mo = vRange.match(crystalVoltage)
                if mo:
                    voltageList = [float(x) for x in mo.groups()[0].split(",")]                    

                # once we moved to the right spot, stay and run the DAQ as many times as it's needed
                for voltage in voltageList:
                    self.logger.debug("Working on crystal %s @ Xpos=%d Ypos=%d, voltage point=%f"%(crystalID, theX, theY, voltage))
                    self.theSequence.append(("setHV", (SlotHV, ChHV, voltage)))
                    self.theSequence.append(("syncState", 0))
                    
                    self.logger.debug("runDAQ")    
                    self.theSequence.append(("runDAQ", (self.sequenceName, theCrystal, SlotDAQ, ChDAQ, crystalID)))
                    self.theSequence.append(("syncState",0))

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

        if not self.hasSequenceConfig:
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
                    if retTokenId in tokenDict:
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
        return peccoCommandCenter.Command(("MoveController", "resetXY"), tokenId=theTokenId, answerQueue=self.inputQueue)

    def moveXY(self, theTokenId, args):
        return peccoCommandCenter.Command(("MoveController", "move_xy", args[0], args[1]), tokenId=theTokenId, answerQueue=self.inputQueue)

    def setHV(self, theTokenId, args):
        self.HVControllerTokenID = theTokenId
        # SlotHV, ChHV, voltage
        return peccoCommandCenter.Command(("HVController", "setHV", args[0], args[1], args[2]), tokenId=theTokenId, answerQueue=self.inputQueue)
        
    def runDAQ(self, theTokenId, args):
        self.logger.debug("runDAQ: ", args) # sequenceName, position, crystalID
        return peccoCommandCenter.Command(("RCController", "runDAQ", args[0], args[1], args[2]), tokenId=theTokenId, answerQueue=self.inputQueue)

        self.logger.debug("runDAQ: ", args) # sequenceName, position, crystalID
        return peccoCommandCenter.Command(("RCController", "runDAQ", args[0], args[1], args[2]), tokenId=theTokenId, answerQueue=self.inputQueue)

    def setHVtoSafe(self, theTokenId, args):
        self.logger.debug("Setting all Vset to safe value (0V)")
        return peccoCommandCenter.Command(("HVController", "setHVtoSafe", args), tokenId=theTokenId, answerQueue=self.inputQueue)

    def turnOnChannels(self, theTokenId, args):
        self.logger.debug("Turning ON all HV channels")
        return peccoCommandCenter.Command(("HVController", "doAllChanOn", args), tokenId=theTokenId, answerQueue=self.inputQueue)

    def turnOffChannels(self, theTokenId, args):
        self.logger.debug("Turning OFF all HV channels")
        return peccoCommandCenter.Command(("HVController", "doAllChanOff", args), tokenId=theTokenId, answerQueue=self.inputQueue)


class SequencerController(peccoBaseModule.BaseModule):
    def __init__(self, logger, configuration):
        peccoBaseModule.BaseModule.__init__(self, logger, configuration)
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
