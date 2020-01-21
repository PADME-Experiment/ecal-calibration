from __future__ import print_function
import copy
import os
import peccoBaseModule
import peccoCommandCenter
import peccoLogger
import shlex
import subprocess
import sys
import threading
import time
import tsdict

if sys.version_info.major == 3:
    import queue as Queue
else:
    import Queue

"""The generic pipeline member process:
    this class can be instantiated direcly if the default behavior is okay
    or specialized, modifying the processing steps (preExec, executeCommand, postExec)
    """
class PipelineProcess(peccoBaseModule.BaseModule):
    def __init__(self, logger, config, tName, command):
        peccoBaseModule.BaseModule.__init__(self, logger, config)
        self.command = command
        self.dataQueue = Queue.Queue()
        self.syncQueue = Queue.Queue()
        self.dataMutex = threading.Lock()
        self.nextStep = []
        self.name = tName
        self.sync = 0
        self.logger = peccoLogger.PadmeLoggerProxy(logger, self.name, level=True)
        self.setCWD()
        self.setLogfiles()
        self.start()

    # this is unused in the PipelineProcess module
    def setupCmdDict(self):
        pass

    def setCWD(self, cwd="./"):
        self.cwd = cwd

    def setLogfiles(self, logfile="", errfile=""):
        tStamp = time.strftime("%Y%m%d-%H%M%S")
        if logfile == "":
            self.processLogfile = "%s.%s.log"%(self.name, tStamp)
        else:
            self.processLogfile = "%s.%s.log"%(logfile, tStamp)

        if errfile == "":
            self.processErrfile = "%s.%s.err"%(self.name, tStamp)
        else:
            self.processErrfile = "%s.%s.err"%(errfile, tStamp)

        self.logger.trace("setting Logfile: %s Errfile: %s"%(self.processLogfile, self.processErrfile))

    def submit(self, data, sync=0):
        self.logger.debug("%s: submitted data %s"%(self.name, data))
        self.dataQueue.put(data)
        self.sync = sync
        if sync>0:
                # wait for the sync signal
                self.syncQueue.get()

    def connect(self, nextStep):
        self.dataMutex.acquire_lock()
        self.nextStep.append(nextStep)
        self.dataMutex.release_lock()

    def stop(self):
        self.submit("")    

    def preExec(self, data):
        self.logger.info("starting %s process for args %s"%(self.command, data))
        newCmdT = copy.copy(self.command)
        if type(data) == type([]):
            newCmd = "%s %s"%(newCmdT, " ".join(data))
        else:
            newCmd = "%s %s"%(newCmdT, data)
        return newCmd

    def executeCommand(self, cmd, data):
        
        theLogfile = open(self.processLogfile, "w")
        theErrfile = open(self.processErrfile, "w")

        self.logger.debug("this is what I'm about to exec: %s"%cmd)
        self.logger.debug("the logfile for the spawned process is: %s"%self.processLogfile)
        self.logger.debug("the error file for the spawned process is: %s"%self.processErrfile)

        cmdL = shlex.split(cmd)
        pProcess = subprocess.Popen(cmdL, cwd=self.cwd, stdout=theLogfile, stderr=theErrfile)
        self.logger.debug("This is the PID of the spawned process: %d"%pProcess.pid)
        self.pPid = pProcess.pid
        self.logger.info("process started with pid %d"%self.pPid)

        notDone = True
        while notDone:
            rv = pProcess.poll()
            if rv != None:
                notDone = False
            else:
                time.sleep(1)

        self.logger.info("process %d finished with return value %d"%(self.pPid, rv))

        theLogfile.close()
        theErrfile.close()
        self.pPid = -1
        return data

    def postExec(self, result, data):
        self.logger.debug("postprocessing data %s"%result)
        newSync = 0
        if self.sync > 0:
                newSync = self.sync-1
        self.dataMutex.acquire_lock()        
        if len(self.nextStep)>0:
            for step in self.nextStep:
                step.submit(result, newSync)
        self.dataMutex.release_lock()

    def run(self):
        while 1:
            data = self.dataQueue.get()
            print(data)
            #self.logger.trace("got data: %s"%data)
            if data != "": 
                cmd = self.preExec(data)
                res = self.executeCommand(cmd, data)
                self.postExec(res, data)
                if self.sync>0:
                        self.syncQueue.put("yeah, yeah, I'm done")
            else: 
                self.postExec("", "")
                break

        self.logger.debug("PipelineProcess %s: exiting..."%self.name)

"""  The actual pipeline:
    contains the pipeline objects and provides an entry point,
    in addition to methods to assemble the pipeline itself.
    """
class ProcessingPipeline(object):
    def __init__(self, name):
        self.name = name
        self.pipelineDict = {}

    def addProcess(self, name, processObj):
        if name not in self.pipelineDict:
            self.pipelineDict[name] = processObj
            return True
        else:
            return False

    def connect(self, fromProc, toProc):
        if fromProc=="entryPoint":
            self.entryPoint = self.pipelineDict[toProc]
        else:
            self.pipelineDict[fromProc].connect(self.pipelineDict[toProc])

    def submit(self, data, sync=0):
        self.entryPoint.submit(data, sync=sync)

    def destroy(self):
        self.entryPoint.submit("")


""" The actual DAQ process """ 
class DAQProcess(PipelineProcess):
    def __init__(self, logger, config, sequenceName, daqWorkDir):
        cmd = "%s -c"%config["PadmeDAQexecutable"]
        PipelineProcess.__init__(self, logger, config, "DAQStep", cmd)
        self.daqWorkDir = daqWorkDir
        self.sequenceName = sequenceName

    def preExec(self, data):
        position, crystalID = data

        # read the HV state
        answer = self.sendSyncCommand("HVController", "getFullChannelData", position)
        self.logger.debug("Response from HVController: %s"%answer)
        self.logger.debug("Data: %s"%answer.args())
        voltage, current, _, setVoltage, setCurrent, _ = [int(x) for x in answer.args()[0]]

        # set the CWD
        self.setCWD(self.daqWorkDir)

        # get the daqConfgiFile
        daqConfigFile = self.config["DAQConfigFiles"][position]

        # create the output fileName
        self.outputFileName = "CrystalTesting_at-%s_CID-%s_Vreal-%s_Ireal-%s_sVset-%s"%(position, crystalID, voltage, current, setVoltage)

        # set the filenames
        logsFileName = "%s/log/%s"%(self.daqWorkDir, self.outputFileName)
        self.setLogfiles(logsFileName, logsFileName)

        self.logger.debug("LogFileName  = %s.log"%logsFileName)
        self.logger.debug("ErrFileName  = %s.err"%logsFileName)
        self.logger.debug("DAQ Work dir = %s"%self.daqWorkDir)

        self.logger.debug("Removing init files from run/")
        os.system("rm %s/run/init*"%self.daqWorkDir)

        theCmd = "%s %s"%(self.command, daqConfigFile)

        self.logger.trace("theCmd: %s"%theCmd)
        self.logger.trace("theCmd: %s"%self.command)
        self.logger.info("submitting DAQ process for sequence %s, crystal %s@%s with %sV"%(self.sequenceName, crystalID, position, voltage))
        return theCmd

    def postExec(self, result, data):

        self.logger.trace("postExec input data: %s %s"%(result, data))

        if result == "" and data == "": 
            super(DAQProcess, self).postExec("", "")
            return

        # check if the DAQ produced an output file
        outputDAQdata = os.popen("ls -1 %s/data/CrystalCheck*"%self.daqWorkDir)
        outputDAQfileCands = outputDAQdata.readlines()
        outputDAQdata.close()

        if len(outputDAQfileCands)>0:
            outputDAQfile = outputDAQfileCands[-1].strip()
        else:
            self.logger.warn("The DAQ does not appear to have produced a data file. Check the log/err files for any problem...")
            self.logger.warn("pipeline processing stops here.")
            return

        self.logger.debug("found the DAQ file: %s"%outputDAQfile)
        dt = outputDAQfile.split("+")[1]
        dataFileName = "%s/data/%s%s"%(self.daqWorkDir, self.outputFileName, dt)

        self.logger.debug("Moving it to: %s"%dataFileName)
        os.system("mv %s %s"%(outputDAQfile, dataFileName))

        # and now we submit the newly acquired file to the next step
        # in the pipeline, if any...
        super(DAQProcess, self).postExec(dataFileName, data)

""" The Level1/Merger process """
class LVL1Process(PipelineProcess):
    def __init__(self, logger, config, daqWorkDir):
        cmd = "%s -n0 -d/ "%config["Level1DAQexecutable"] #, daqWorkDir) filename to process have absolute path
        PipelineProcess.__init__(self, logger, config, "Level1Step", cmd)
        self.daqWorkDir = daqWorkDir
        self.setCWD(self.daqWorkDir)

    def mkLogFileName(self, info): 
        fname = info.split("/")[-1]
        logFileName = "%s/log/%s.l1"%(self.daqWorkDir, fname)
        self.setLogfiles(logFileName, logFileName)

    def preExec(self, data):
        self.logger.info("starting %s process for args %s"%(self.command, data))
        if type(data)==type([]):
            processFile = "%s.l1input.txt"%data[0]
            lf = open(processFile, "w")
            for entry in range(len(data)):
                lf.write("%d %s\n"%(entry, data[entry]))
            lf.close()
            newCmd = "%s -l %s -o %s.l1"%(self.command, processFile, data[0])
            self.mkLogFileName(data[0])
        else: 
            processFile = "%s.l1input.txt"%data
            self.logger.trace("processFile: %s"%processFile)
            lf = open(processFile, "w")
            lf.write("0 %s\n"%data)
            lf.close()
            newCmd = "%s -l %s -o %s.l1"%(self.command, processFile, data)
            self.mkLogFileName(data)

        self.logger.debug("The command to execute: %s"%newCmd)
        return newCmd

""" The Analysis process """ 
class AnalysisProcess(PipelineProcess):
    def __init__(self, logger, config, daqWorkDir):
        cmd = "%s -i "%config["AnalysisDAQexecutable"]
        PipelineProcess.__init__(self, logger, config, "AnalysisStep", cmd)
        self.daqWorkDir = daqWorkDir
        self.analysisWorkDir = "%s/data/"%self.daqWorkDir
        self.setCWD(self.analysisWorkDir)
        

    def preExec(self, data):
        self.logger.info("starting %s process for args %s"%(self.command, data))
        info = data.split("/")
        fname = info[-1]
        logFileName = "%s/log/%s.analysis"%(self.daqWorkDir, fname)
        self.setLogfiles(logFileName, logFileName)
        realDataFile = "%s.l1.root"%fname
        self.logger.info("Processing file: %s"%realDataFile)
        return super(AnalysisProcess, self).preExec(realDataFile)

    def postExec(self, result, data):
        self.logger.debug("postprocessing data %s, %s"%(result, data))
        if result == "" and data == "": 
            super(AnalysisProcess, self).postExec("", "")
            return

        try:
                tf = open("%s/out.root"%self.analysisWorkDir).close()
        except IOError:
                self.logger.debug("couldn't find file out.root. Maybe there was a problem with the Analysis step? Check the log/err files.")
        else:
                outFile = "%s.out.root"%result
                self.logger.info("renaming out.root to %s"%outFile)
                os.system("mv %s/out.root %s"%(self.analysisWorkDir, outFile))
                super(AnalysisProcess, self).postExec(outFile, data)
