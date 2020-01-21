import peccoBaseModule
import os
import time

def HVchannel(position, *discard):
    #position = positionTuple[0]
    pY, pX = [int(x) for x in position]
    return pY*5+pX

class HVController(peccoBaseModule.BaseModule):
    def __init__(self, logger, configuration):
        peccoBaseModule.BaseModule.__init__(self, logger, configuration)
        self.name = "HVController"
        self.setupLoggerProxy()

        self.safeReadMode = False
        if "HVSafeReadMode" in self.config and self.config["HVSafeReadMode"] == "True":
            self.safeReadMode = True

        self.syncMode = False
        if "HVVSetSyncMode" in self.config and self.config["HVVSetSyncMode"] == "True":
            self.syncMode = True

        self.logger.debug("HVcontroller configuration => HVSafeReadMode: %s - HVVSetSyncMode: %s"%(self.safeReadMode, self.syncMode))

        #self.hvExec = "/home/daq/PCT_HV/PCT_HV -s 0 -c %d %s 2>/dev/null"
        #self.globalHvExec = "/home/daq/PCT_HV/PCT_HV -C %s 2>/dev/null"
        self.hvExec = "%s -s 0 -c %%d %%s 2>/dev/null"%self.config["HVexecutable"]
        self.globalHvExec = "%s -C %%s 2>/dev/null"%self.config["HVexecutable"]
        self.failureTime = int(self.config["HVFailureTime"])
        if self.failureTime>0:
            self.logger.debug("HVFailureTime set to %d"%self.failureTime)
        # Format
        self.HVMap = [(False, 0., 0., 0., 0., 0., 0.) for x in range(25)]
        # here I should put the code to interrogate the HV system
        # and collect info about its current status
        self.loadCurrentStatus()
        # Also this class needs a watchdog to monitor the status
        # of HV channels and eventually raise alarms

    def grabGlobalData(self, cmd):
        #retry = True
        while True:
            dataChan = os.popen(self.globalHvExec%cmd)
            info = dataChan.readlines()
            dataChan.close()
            if len(info)>0:
                return info[-1].strip().split()
            elif self.safeReadMode:
                time.sleep(0.1)
            else:
                return [-424242]*25 

    def execPCT_HV(self, position, command):
        self.logger.debug("Argument to execPCT_HV: %s"%position)
        channel = HVchannel(position)
        self.logger.debug("Working on HV channel: %d"%channel)
       
        while True:
            dataChan = os.popen(self.hvExec%(channel, command))
            data = dataChan.readlines()
            dataChan.close()
            self.logger.debug(data)
    
            if len(data)>0:
                return data[-1]
            elif self.safeReadMode:
                time.sleep(0.1)
            else:
                return "-424242"

    def setupCmdDict(self):
        self.cmdDict["checkHV"] = self.checkHV
        self.cmdDict["setHVfake"] = self.setHVfake
        self.cmdDict["setHV"] = self.setHV
        self.cmdDict["turnOnChannel"] = self.turnOnChannel
        self.cmdDict["turnOffChannel"] = self.turnOffChannel
        self.cmdDict["status"] = self.status
        self.cmdDict["readVoltage"] = self.readVoltage
        self.cmdDict["readCurrent"] = self.readCurrent
        self.cmdDict["readImpedance"] = self.readImpedance
        self.cmdDict["getChannelData"] = self.getChannelData
        self.cmdDict["readSetVoltage"] = self.readSetVoltage
        self.cmdDict["readSetCurrent"] = self.readSetCurrent
        self.cmdDict["readSetImpedance"] = self.readSetImpedance
        self.cmdDict["getSetChannelData"] = self.getSetChannelData
        self.cmdDict["getFullChannelData"] = self.getFullChannelData
        self.cmdDict["allHVStatus"] = self.loadCurrentStatus
        self.cmdDict["doAllChanOn"] = self.doAllChanOn
        self.cmdDict["doAllChanOff"] = self.doAllChanOff
        self.cmdDict["setHVtoSafe"] = self.setHVtoSafe

    def loadCurrentStatus(self):
        powerInfo = self.grabGlobalData("-S")
        currentVoltageInfo = self.grabGlobalData("-V")
        currentCurrentInfo = self.grabGlobalData("-I")
        currentImpedanceInfo = self.grabGlobalData("-Z")                
        setVoltageInfo = self.grabGlobalData("-v")
        setCurrentInfo = self.grabGlobalData("-i")
        setImpedanceInfo = self.grabGlobalData("-z")
        for x in range(25):
            self.HVMap[x] = (
                    powerInfo[x],
                    float(currentVoltageInfo[x]),
                    float(currentCurrentInfo[x]),
                    float(currentImpedanceInfo[x]),
                    float(setVoltageInfo[x]),
                    float(setCurrentInfo[x]),
                    float(setImpedanceInfo[x]) 
                    )
            self.logger.debug("%02d"%x, self.HVMap[x])


    def doAllChanOn(self, *discard):
        return self.grabGlobalData("-1")

    def doAllChanOff(self, *discard):
        return self.grabGlobalData("-0")

    def setHVtoSafe(self, *discard):
        return self.grabGlobalData("-H 0")

    def checkHV(self, position, *discard):
        status = self.status(position)
        if status == "Off":
            self.logger.warn("Channel %d for position %s is currently OFF"%(HVchannel(position), position))
            return False
        
        if self.failureTime > 0:
            startTime = time.time()

        voltage = -424242
        setVoltage = -424242
        delta = 1. # Volt, 'cause Paolo said so.

        while setVoltage == -424242:
            if (time.time()-startTime)>self.failureTime:
                self.logger.warn("Channel %d [pos: %s] was unable to reach voltage %s within %d s. Giving up."
                    %(HVchannel(position), position, setVoltage, self.failureTime))
                return False
        
            time.sleep(0.5)
            setVoltage = self.readSetVoltage(position)

        while voltage == -424242 or abs(voltage-setVoltage)>delta:
            if (time.time()-startTime)>self.failureTime:
                self.logger.warn("Channel %d [pos: %s] was unable to reach voltage %s within %d s. Giving up."
                    %(HVchannel(position), position, setVoltage, self.failureTime))
                return False

            time.sleep(0.5)
            voltage = self.readVoltage(position)

        return True

    def getChannelData(self, position, *discard):
        v = self.readVoltage(position)
        i = self.readCurrent(position)
        z = self.readImpedance(position)
        return (v,i,z)

    def getSetChannelData(self, position, *discard):
        v = self.readSetVoltage(position)
        i = self.readSetCurrent(position)
        z = self.readSetImpedance(position)
        return (v,i,z)

    def getFullChannelData(self, position, *discard):
        dataCurrent = self.getChannelData(position)
        dataSet     = self.getSetChannelData(position)
        return dataCurrent+dataSet

    def setHVfake(self, position, index, *discard):
        return self.setHV((int(index), 1, 1, position, "pippo", [452, 557, 623, 717, 884]))

    def setHV(self, args):
        self.logger.debug(args)
        vPoint, posX, posY, position, crystalID, voltages = args
        vPoint = int(vPoint)
        self.logger.debug(vPoint, posX, posY, position, crystalID, voltages)
        
        self.logger.debug("here I'm supposed to set the HV channel for crystal %s to the specified value of %d"%(crystalID, voltages[vPoint]))
        self.logger.debug("The relevant channelId is: %d "%HVchannel(position))
        answer1 = self.execPCT_HV(position, "-H %d"%voltages[vPoint])
        answer2 = None
        if self.syncMode == True:
            answer2 = self.checkHV(position)
        return (answer1, answer2)

    def turnOnChannel(self, position, *discard):
        return self.execPCT_HV(position, "-1")

    def turnOffChannel(self, position, *discard):
        return self.execPCT_HV(position, "-0")
    
    def status(self, position, *discard):
        return self.execPCT_HV(position, "-S")

    def readVoltage(self, position, *discard):
        return float(self.execPCT_HV(position, "-V"))

    def readCurrent(self, position, *discard):
        return float(self.execPCT_HV(position, "-I"))

    def readImpedance(self, position, *discard):
        return float(self.execPCT_HV(position, "-Z"))

    def readSetVoltage(self, position, *discard):
        return float(self.execPCT_HV(position, "-v"))

    def readSetCurrent(self, position, *discard):
        return float(self.execPCT_HV(position, "-i"))

    def readSetImpedance(self, position, *discard):
        return float(self.execPCT_HV(position, "-z"))
