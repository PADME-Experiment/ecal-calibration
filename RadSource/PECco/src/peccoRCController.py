import peccoBaseModule
import peccoCommandCenter
import peccoProcessingPipeline as peccoPP

class RCController(peccoBaseModule.BaseModule):
    def __init__(self, logger, configuration):
        peccoBaseModule.BaseModule.__init__(self, logger, configuration)
        self.name = "RCController"
        self.nakedLogger = logger
        self.setupLoggerProxy()
        self.status = "idle"
        self.ppDict = {}

    def setupCmdDict(self):
        self.cmdDict["status"]    = self.status
        self.cmdDict["runDAQ"]    = self.runDAQ

    def getProcessingPipeline(self, sequenceName, daqWorkDir):
        if sequenceName in self.ppDict:
            return self.ppDict[sequenceName]

        pp = peccoPP.ProcessingPipeline(sequenceName)

        pps1 = peccoPP.DAQProcess(self.nakedLogger, self.config, sequenceName, daqWorkDir)
        pps2 = peccoPP.LVL1Process(self.nakedLogger, self.config, daqWorkDir)
        pps3 = peccoPP.AnalysisProcess(self.nakedLogger, self.config, daqWorkDir)

        pps1.setCommandQueue(self.commandQueue)
        pps2.setCommandQueue(self.commandQueue)
        pps3.setCommandQueue(self.commandQueue)

        pp.addProcess("DAQ",      pps1)
        pp.addProcess("LVL1",     pps2)
        pp.addProcess("ANALYSIS", pps3)

        pp.connect("entryPoint", "DAQ")
        pp.connect("DAQ", "LVL1")
        pp.connect("LVL1", "ANALYSIS")

        self.ppDict[sequenceName] = pp
        return pp

    def exit(self):
        for entry in self.ppDict:
            self.ppDict[entry].submit("")
        super(RCController, self).exit()

    def status(self):
        if self.status == "running":
            return "DAQ running with pid %d"%self.daqPid
        else:
            return "DAQ idle"

    def runDAQ(self, sequenceName, position, crystalID):
        daqWorkDir = "%s/%s"%(self.config["DAQConfigPath"], sequenceName)
        thePP = self.getProcessingPipeline(sequenceName, daqWorkDir)
        # the 1 indicates that we wish to wait for the first step of the processing pipeline to be finished before continuing
        thePP.submit((position, crystalID), 1) 
        return "DAQ done"
