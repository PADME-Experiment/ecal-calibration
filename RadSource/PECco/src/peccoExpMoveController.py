import peccoBaseModule
import httplib
import socket
import time

# this version of the MoveController deals with the motor
# system engineered for online calibration within the experiment

class ExpMoveController(peccoBaseModule.BaseModule):
    def __init__(self, logger, configuration):
        peccoBaseModule.BaseModule.__init__(self, logger, configuration)
        self.name = "ExpMoveController"
        self.setupLoggerProxy()
        self.remoteMoveAddress = self.config["ExpMovementServer"]
        self.remoteMovePort    = self.config["ExpMovementPort"]
        self.currentX = 0
        self.currentY = 0
        self.mcOffline = True   
        # read in the data file and prepare the crystal dictionary
        self.readInData()
        
        # setup TCP connection with movement server
        self.connectToServer()
        

        data = self.read_position()
        self.logger.trace("%s"%" ".join([str(x).strip() for x in data]))
        if not self.mcOffline:
                pos = data[0][2].split(";")
                self.currentX = int(pos[0].split("=")[1])
                self.currentY = int(pos[1].split("=")[1])

    def setupCmdDict(self):
        self.cmdDict["read_position"] = self.read_position
        self.cmdDict["idle"]          = self.idle
        self.cmdDict["set_zero"]      = self.set_zero
        self.cmdDict["set_xabs"]      = self.set_xabs
        self.cmdDict["set_yabs"]      = self.set_yabs
        self.cmdDict["move_xy"]       = self.move_xy
        self.cmdDict["set_xy"]        = self.set_xy
        self.cmdDict["resetx"]        = self.resetx
        self.cmdDict["resety"]        = self.resety
        self.cmdDict["resetXY"]       = self.resetXY
        self.cmdDict["set_xrel"]      = self.set_xrel
        self.cmdDict["set_yrel"]      = self.set_yrel
            
    def connectToServer(self):
        self.serverSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        try:
            self.serverSocket.connect((self.remoteMoveAddress, self.remoteMovePort))
        except ConnectionRefused as ss:
            msg = "ExpMovement Server refused connection: %s"%ss
            self.logger.want(msg)
            self.moduleStatus = False
            self.moduleErrorMessage = msg
        except Exception as exc:
            msg = "Major problem in ExpMoveController: %s"%exc
            self.logger.warn(msg)
            self.moduleStatus = False
            self.moduleErrorMessage = msg
    
    def writeReadTcp(self, msg):
    
        
    def httpGet(self, *getData):
        gd = "/".join(getData)
        self.logger.debug(gd)
        self.logger.debug(self.arduinoAddress)
        if "SimulationMode" in self.config and self.config["SimulationMode"] == "True":
                return (200, "OK", "This is just a simulation...")

        conn = httplib.HTTPConnection(self.arduinoAddress)
        try:
                conn.connect()
        except socket.error as se:
                self.logger.warn("There was a problem connecting to the Arduino Move Controller: %s"%se)
                self.mcOffline = True
                return "Error connecting to Arduino Controller!"

        self.mcOffline = False
        conn.request("GET", "/arduino/%s"%gd)  
        time.sleep(0.4)
        response = conn.getresponse()
        data2return = (response.status, response.reason, response.read())
        self.logger.trace("%d %s %s"%data2return)
        conn.close()
        return data2return

    # Arduino commands --
    # Here we assume that the zero position corresponds to the source
    # being correctly centered on the crystal (0,0).
    # All movements shall be considered relative to this position!
    def set_zero(self):
        info = self.httpGet("zero", "ok")
        if not self.mcOffline:
                self.currentX = 0
                self.currentY = 0
        return info
    
    # read the current position from Arduino and the one stored in the class
    def read_position(self):
        
        info = self.httpGet("leggi", "ok")
        return (info, (self.currentX, self.currentY))

    def move_xy(self, xval, yval):
        self.logger.trace("Move XY: %d,%d"%(xval, yval))
        info1 = self.set_xabs(xval, True)
        while not self.idle():
                time.sleep(0.1)
        info2 = self.set_yabs(yval, True)
        while not self.idle():
                time.sleep(0.1)
        return (info1, info2)

    def sync_move(self, info, sync):
        info = (200, 'OK', '')
        if sync and not self.mcOffline:
                while(not self.idle()):
                        time.sleep(0.1)
        return info

    # set the absolute position along the X axis
    def set_xabs(self, value, sync=False):
        info = self.httpGet("xabs", str(value))
        if not self.mcOffline:
                self.currentX = value
                return self.sync_move(info, sync)
        else:
                return (500, "Movement Controller Server offline", '')

    # set the absolute position along the Y axis
    def set_yabs(self, value, sync=False):
        info = self.httpGet("yabs", str(value))
        if not self.mcOffline:
                self.currentY = value
                return self.sync_move(info, sync)
        else:
                return (500, "Movement Controller Server offline", '')

    # set the relative position along the X axis
    def set_xrel(self, value, sync=False):
        newXpos = self.currentX + int(value)
        if newXpos<0: newXpos = 0
        if newXpos>100000: newXpos = 100000
        return self.set_xabs(newXpos, sync)

    # set the relative position along the X axis
    def set_yrel(self, value, sync=False):
        newYpos = self.currentY + int(value)
        if newYpos<0: newYpos = 0
        if newYpos>100000: newYpos = 100000
        return self.set_yabs(newYpos, sync)
        
    # Assigns coords to the current position of the motors, no movement is implied
    def set_xy(self, xval, yval):
        info = self.httpGet("setxy", str(xval), str(yval))
        if not self.mcOffline:
                self.currentX = xval
                self.currentY = yval
        return info
    
    # move back to zero along X axis
    def resetx(self):
        info = self.httpGet("resetx", "ok")
        if not self.mcOffline:
                self.currentX = 0
        return info

    # move back to zero along Y axis
    def resety(self):
        info = self.httpGet("resety", "ok")
        if not self.mcOffline:
                self.currentY = 0
        return info

    # combine the two resets from above
    def resetXY(self):
        infoX = self.httpGet("resetx", "ok")
        infoY = self.httpGet("resety", "ok")
        return (infoX, infoY)

    def idle(self):
        # the arduino Yun HTTP server times out when the controller 
        # is busy moving the step-by-step motors, returning a 500 OK HTTP 
        # response. 200 OK is returned upon success
        response = self.read_position()

        if self.mcOffline:
                return True
        elif response[0][0] == 200:
                return True
        else:
                return False
