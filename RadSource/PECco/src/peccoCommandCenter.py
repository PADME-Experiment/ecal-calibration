import peccoBaseModule
import tsdict

class Command(object):
    def __init__(self, command=(), tokenId=42, answerQueue=None):
        self.cmd = command
        self.token = str(tokenId)

        if answerQueue:
            self.wantAnswer = True
            self.answerQueue = answerQueue
        else:
            self.wantAnswer = False

    def __repr__(self):
        if self.wantAnswer:
            strRepr = "Command(To:%s, Cmd:%s, Args:%s, TokenId:%d, AnswerQueue:%s)"%(self.receiver(), self.command(), self.args(), self.tokenId(), self.answerQueue)
        else:
            strRepr = "Command(To:%s, Cmd:%s, Args:%s, TokenId:%s)"%(self.receiver(), self.command(), self.args(), self.tokenId())
        return strRepr

    def __str__(self):
        return repr(self)

    def receiver(self):
        if len(self.cmd) < 1:
            return "Receiver:absent "
        return self.cmd[0]

    def command(self):
        if len(self.cmd) < 2:
            return "Command:absent"
        return self.cmd[1]

    def tokenId(self):
        return self.token

    def args(self):
        if len(self.cmd)<2:
            return []
        return self.cmd[2:]
        
    def answer(self, sender, *theAnswer):
        theRealAnswer = (sender, "answer") + theAnswer
        answerMessage = Command(theRealAnswer, tokenId=self.tokenId())
        if self.wantAnswer:
            self.answerQueue.put(answerMessage)

class CommandCenter(peccoBaseModule.BaseModule):
    def __init__(self, logger, configuration):
        peccoBaseModule.BaseModule.__init__(self, logger, configuration)
        self.registeredModules = tsdict.TSDict()
        self.name = "CommandCenter"
        self.setupLoggerProxy()

    def setupCmdDict(self):
        self.cmdDict["addModule"]  = self.addModule
        self.cmdDict["rmModule"]   = self.rmModule
        self.cmdDict["moduleList"] = self.moduleList

    def addModule(self, name, module):
        self.logger.debug(self.name, "- addModule: ", name)
        module.setCommandQueue(self.inputQueue)
        self.registeredModules[name] = module

    def rmModule(self, name):
        self.logger.debug(self.name, "- rmModule: ", name)
        if name in self.registeredModules:
            del(self.registeredModules[name])

    def moduleList(self, *args):
        return self.registeredModules.keys()
    
    def sendCommand(self, cmd):
        self.inputQueue.put(cmd)

    def run(self):
        self.logger.debug(self.name+" starting...")
        self.goOn = True
        while self.goOn:
            try:
                cmd = self.inputQueue.get(timeout=1)
            except peccoBaseModule.Queue.Empty:
                continue

            dest = cmd.receiver()
            self.logger.debug("Received message to", cmd.receiver(), cmd.command(), cmd.tokenId())
            if dest == "CommandCenter":
                self.processCommand(cmd)
            else:
                if dest in self.registeredModules:
                    module = self.registeredModules[dest]
                    module.inputQueue.put(cmd)
                else:
                    self.logger.error(self.name, ": Whoops, module", dest, "not found...")
        print(self.name, "farewell...")
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       
    # def processCommand(self, cmd): 
    #     theCommand = cmd.command()
    #     if theCommand == "exit":
    #         self.goOn = False
    #     elif theCommand in self.cmdDict:
    #         result = self.cmdDict[theCommand](cmd)
    #         cmd.answer(self.name, result)
    #     else:
    #         self.logger.warn(self.name, "Command %s not found. Ignoring."%theCommand)
