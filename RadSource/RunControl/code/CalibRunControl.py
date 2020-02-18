#!/usr/bin/python

import os
import time
import socket
import sys
import re
import signal
import daemon
import getopt

from Run     import Run
from Logger  import Logger

class RunControlServer:

    def __init__(self,mode):

        # Get position of DAQ main directory from PADME_DAQ_DIR environment variable
        # Default to current dir if not set
        self.daq_dir = os.getenv('PADME_CALIB_DIR',".")

        # Get port to use for RC connection from PADME_RC_PORT or use port 10000 as default
        self.runcontrol_port = int(os.getenv('PADME_RC_PORT',"10000"))

        # Define id file for passwordless ssh command execution
        self.ssh_id_file = "%s/.ssh/id_rsa_daq"%os.getenv('HOME',"~")

        # Define names of lock and last_used_setup files
        self.lock_file = self.daq_dir+"/run/lock"

        # Redefine print to send output to log file
        sys.stdout = Logger()
        sys.stderr = sys.stdout
        if mode == "i": sys.stdout.interactive = True

        # Create lock file
        if (self.create_lock_file() == "error"): exit(1)

        # Define name of common calibration setup
        self.calib_setup = "calibration"

        print "=== Starting PADME Calibration Run Control server"

        # Create run (default to board 0 channel 0)
        self.run = Run()
        if (self.run.change_setup(self.calib_setup) == "error"):
            print "ERROR - Error while setting run setup to %s"%self.calib_setup
            if os.path.exists(self.lock_file): os.remove(self.lock_file)
            exit(1)

        # Start in idle state
        self.current_state = "idle"

        # Set list of possible run types
        self.run_type_list = ['CALIBRATION']
        print "--- Known run types ---"
        print self.run_type_list

        # Create useful regular expressions to parse user commands
        self.re_get_board_config_daq = re.compile("^get_board_config_daq (\d+)$")
        self.re_get_board_config_zsup = re.compile("^get_board_config_zsup (\d+)$")
        self.re_get_board_log_file_daq = re.compile("^get_board_log_file_daq (\d+)$")
        self.re_get_board_log_file_zsup = re.compile("^get_board_log_file_zsup (\d+)$")
        self.re_change_crystal = re.compile("^change_crystal\s+(\d+)\s+(\d+)$")
        self.re_change_hv = re.compile("^change_hv\s+(\d+)$")

        # Create a TCP/IP socket
        if not self.create_socket():
            print "=== RunControlServer - ERROR while creating socket. Exiting"
            self.final_cleanup()
            exit(1)

        self.sock.listen(1)

        # Define SIGINT handler
        signal.signal(signal.SIGINT,self.sigint_handler)

        # Setup main interface
        ret_main = self.main_loop()

        # Reset SIGINT handler to default
        signal.signal(signal.SIGINT,signal.SIG_DFL)

        # Handle server exit procedure
        ret_exit = self.server_exit()

        # Clean up before exiting
        self.final_cleanup()

        if ( ret_main == "error" or ret_exit == "terminate_error" ): exit(1)

        exit(0)

    def create_socket(self):

        # Create a TCP/IP socket
        self.sock = socket.socket(socket.AF_INET,socket.SOCK_STREAM)

        # Allow immediate reuse of socket after server restart (i.e. disable socket TIME_WAIT)
        self.sock.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)

        # Bind the socket to the port
        server_address = ('localhost',self.runcontrol_port)
        print "Starting server socket on %s port %s"%server_address
        try:
            self.sock.bind(server_address) # Listen for incoming connections
        except:
            print "ERROR - Could not bind to socket: %s"%str(sys.exc_info()[0])
            return False

        return True

    def final_cleanup(self):

        # Final clean up procedure before exiting
        if self.sock:
            self.sock.shutdown(socket.SHUT_RDWR)
            self.sock.close()
        if os.path.exists(self.lock_file): os.remove(self.lock_file)

    def create_lock_file(self):

        # Check if lock file exists
        if (os.path.exists(self.lock_file)):
            if (os.path.isfile(self.lock_file)):
                pid = 0
                with open(self.lock_file,"r") as lf:
                    for ll in lf: pid = ll

                print "Lock file %s found for pid %s - checking status"%(self.lock_file,pid)

                ppinfo = os.popen("ps -p %s"%pid)
                pinfo = ppinfo.readlines()
                ppinfo.close()
                if len(pinfo)==2:
                    if pinfo[1].find("<defunct>")>-1:
                        print "There is zombie process with this pid. The RunControlServer is probably dead. Proceeding cautiously..."
                    else:
                        print "ERROR - there is already a RunControlServer running with pid %s"%pid
                        return "error"
                else:
                    print "No RunControlServer process found. As you were..."
            else:
                print "ERROR - Lock file %s found but it is not a file"%self.lock_file
                return "error"

        # Create our own lock file
        pid = os.getpid()
        with open(self.lock_file,"w") as lf: lf.write("%d\n"%pid)

        return "ok"

    def sigint_handler(self,signal,frame):

        print "=== RunControlSever received SIGINT: exiting"

        # If a run is initialized/running, abort it as cleanly as possible
        ret_exit = "terminate_ok"
        if ( self.current_state == "initialized" or self.current_state == "running" ):
            self.run.run_comment_end = "Run aborted due to RunControlServer SIGINT"
            ret_exit = self.terminate_run()

        # Clean up before exiting
        self.final_cleanup()

        if ( ret_exit == "terminate_error" ): exit(1)

        exit(0)

    def server_exit(self):

        # Procedure to gracefully handle server shutdown

        print "=== RunControlSever is shutting down"

        # If a run is initialized/running, abort it as cleanly as possible
        if ( self.current_state == "initialized" or self.current_state == "running" ):
            self.run.run_comment_end = "Run aborted due to RunControlServer shutdown"
            return self.terminate_run()

        # Otherwise nothing to do
        return "ok"

    def main_loop(self):

        while True:

            # Wait for a connection
            print "Waiting for a connection"
            (self.connection,client_address) = self.sock.accept()
            print "Connection from %s"%str(client_address)

            while True:

                # Handle connection according to curren status of RunControl
                if self.current_state == "idle":
                    new_state = self.state_idle()
                elif self.current_state == "initialized":
                    new_state = self.state_initialized()
                elif self.current_state == "running":
                    new_state = self.state_running()
                elif self.current_state == "initfail":
                    new_state = self.state_initfail()
                else:
                    print "ERROR: unknown state %s - ABORTING"%self.current_state
                    new_state = "exit"

            # See if status changed
                if new_state == "idle" or new_state == "initialized" or new_state == "running" or new_state == "initfail":

                    self.current_state = new_state

                else:

                    self.connection.close()

                    if new_state == "exit":
                        print "=== RunControlSever received shutdown command: exiting"
                        return "exit"
                    elif new_state != "client_close":
                        print "=== RunControlServer = ERROR: unknown new state %s - ABORTING"%new_state
                        return "error"

                    # Exit from client handling loop and wait for a new client
                    break

    def state_idle(self):

        # Receive and process commands for "idle" state
        while True:

            cmd = self.get_command()
            if (cmd == "client_close"):
                return "client_close"
            elif (cmd == "get_state"):
                self.send_answer(self.current_state)
            elif (cmd == "get_board_list"):
                self.send_answer(str(self.run.boardid_list))
            elif (cmd == "get_trig_config"):
                self.send_answer(self.get_trig_config())
            elif (cmd == "new_run"):
                res = self.new_run()
                if (res == "client_close"):
                    return "client_close"
                elif (res == "error"):
                    print "ERROR while initializing new run"
                elif (res == "initialized"):
                    return "initialized"
                elif (res == "initfail"):
                    return "initfail"
                else:
                    print "ERROR: new_run returned unknown answer %s (?)"%res
            elif (cmd == "shutdown"):
                self.send_answer("exiting")
                return "exit"
            elif (cmd == "help"):
                msg = """Available commands:
help\t\t\t\tShow this help
get_state\t\t\tShow current state of RunControl
get_board_list\t\t\tShow list of boards in use with current setup
get_board_config_daq <b>\tShow current configuration of board DAQ process <b>
get_board_config_zsup <b>\tShow current configuration of board ZSUP process <b>
get_trig_config\t\t\tShow current configuration of trigger process
change_crystal <board> <chnl>\tChange to crystal connected to <board> <chnl>
change_hv <hv>\t\t\tChange HV to <hv>
new_run\t\t\t\tInitialize system for a new run
shutdown\t\t\tTell RunControl server to exit (use with extreme care!)"""
                self.send_answer(msg)
            else:

                # See if command can be handled by a regular expression
                found_re = False

                m = self.re_get_board_config_daq.match(cmd)
                if (m):
                    self.send_answer(self.get_board_config_daq(int(m.group(1))))
                    found_re = True

                m = self.re_get_board_config_zsup.match(cmd)
                if (m):
                    self.send_answer(self.get_board_config_zsup(int(m.group(1))))
                    found_re = True

                m = self.re_change_crystal.match(cmd)
                if (m):
                    self.send_answer(self.change_crystal(int(m.group(1)),int(m.group(2))))
                    found_re = True

                m = self.re_change_hv.match(cmd)
                if (m):
                    self.send_answer(self.change_hv(int(m.group(1))))
                    found_re = True

                # No regular expression matched: command is unknown
                if not found_re:
                    self.send_answer("unknown command")
                    print "Command %s is unknown"%cmd

    def state_initialized(self):

        # Receive and process commands for "initialized" state
        while True:

            cmd = self.get_command()
            if (cmd == "client_close"):
                return "client_close"
            elif (cmd == "get_state"):
                self.send_answer(self.current_state)
            elif (cmd == "get_board_list"):
                self.send_answer(str(self.run.boardid_list))
            elif (cmd == "get_trig_config"):
                self.send_answer(self.get_trig_config())
            elif (cmd == "get_trig_log"):
                self.send_answer(self.get_trig_log())
            elif (cmd == "abort_run"):
                return self.abort_run()
            elif (cmd == "start_run"):
                return self.start_run()
            elif (cmd == "shutdown"):
                self.send_answer("exiting")
                return "exit"
            elif (cmd == "help"):
                msg = """Available commands:
help\t\t\t\tShow this help
get_state\t\t\tShow current state of RunControl
get_board_list\t\t\tShow list of boards in use with current setup
get_board_config_daq <b>\tShow current configuration of board DAQ process<b>
get_board_config_zsup <b>\tShow current configuration of board ZSUP process<b>
get_board_log_file_daq <b>\tGet name of log file for board DAQ process<b>
get_board_log_file_zsup <b>\tGet name of log file for board ZSUP process<b>
get_trig_config\t\t\tShow current configuration of trigger process
get_trig_log\t\t\tGet name of log file for trigger process
start_run\t\t\t\tStart run
abort_run\t\t\t\tAbort run
shutdown\t\t\tTell RunControl server to exit (use with extreme care!)"""
                self.send_answer(msg)

            else:

                # See if command can be handled by a regular expression
                found_re = False

                m = self.re_get_board_config_daq.match(cmd)
                if (m):
                    self.send_answer(self.get_board_config_daq(int(m.group(1))))
                    found_re = True

                m = self.re_get_board_config_zsup.match(cmd)
                if (m):
                    self.send_answer(self.get_board_config_zsup(int(m.group(1))))
                    found_re = True

                m = self.re_get_board_log_file_daq.match(cmd)
                if (m):
                    self.send_answer(self.get_board_log_file_daq(int(m.group(1))))
                    found_re = True

                m = self.re_get_board_log_file_zsup.match(cmd)
                if (m):
                    self.send_answer(self.get_board_log_file_zsup(int(m.group(1))))
                    found_re = True

                # No regular expression matched: command is unknown
                if not found_re:
                    self.send_answer("unknown command")
                    print "Command %s is unknown"%cmd

    def state_running(self):

        # Receive and process commands for "running" state
        while True:

            cmd = self.get_command()
            if (cmd == "client_close"):
                return "client_close"
            elif (cmd == "get_state"):
                self.send_answer(self.current_state)
            elif (cmd == "get_board_list"):
                self.send_answer(str(self.run.boardid_list))
            elif (cmd == "get_trig_config"):
                self.send_answer(self.get_trig_config())
            elif (cmd == "get_trig_log"):
                self.send_answer(self.get_trig_log())
            elif (cmd == "stop_run"):
                return self.stop_run()
            elif (cmd == "shutdown"):
                self.send_answer("exiting")
                return "exit"
            elif (cmd == "help"):
                msg = """Available commands:
help\t\t\t\tShow this help
get_state\t\t\tShow current state of RunControl
get_board_list\t\t\tShow list of boards in use with current setup
get_board_config_daq <b>\tShow current configuration of board DAQ process<b>
get_board_config_zsup <b>\tShow current configuration of board ZSUP process<b>
get_board_log_file_daq <b>\tGet name of log file for board DAQ process<b>
get_board_log_file_zsup <b>\tGet name of log file for board ZSUP process<b>
get_trig_config\t\t\tShow current configuration of trigger process
get_trig_log\t\t\tGet name of log file for trigger process
stop_run\t\t\t\tStop the run
shutdown\t\t\tTell RunControl server to exit (use with extreme care!)"""
                self.send_answer(msg)

            else:

                # See if command can be handled by a regular expression
                found_re = False

                m = self.re_get_board_config_daq.match(cmd)
                if (m):
                    self.send_answer(self.get_board_config_daq(int(m.group(1))))
                    found_re = True

                m = self.re_get_board_config_zsup.match(cmd)
                if (m):
                    self.send_answer(self.get_board_config_zsup(int(m.group(1))))
                    found_re = True

                m = self.re_get_board_log_file_daq.match(cmd)
                if (m):
                    self.send_answer(self.get_board_log_file_daq(int(m.group(1))))
                    found_re = True

                m = self.re_get_board_log_file_zsup.match(cmd)
                if (m):
                    self.send_answer(self.get_board_log_file_zsup(int(m.group(1))))
                    found_re = True

                # No regular expression matched: command is unknown
                if not found_re:
                    self.send_answer("unknown command")
                    print "Command %s is unknown"%cmd
        return "idle"

    def state_initfail(self):

        # Here we will insert some cleanup code to handle failed initialization
        return "idle"

    def get_command(self):

        # First get length of string
        l = ""
        for i in range(5): # Max 99999 characters
            ch = self.connection.recv(1)
            if ch:
                l += ch
            else:
                print "Client closed connection"
                return "client_close"
        ll = int(l)

        # Then read the right amount of characters from the socket
        cmd = ""
        for i in range(ll):
            ch = self.connection.recv(1)
            if ch:
                cmd += ch
            else:
                print "Client closed connection"
                return "client_close"

        print "Received command %s"%cmd
        return cmd

    def send_answer(self,answer):

        if len(answer)<100000:
            print "Sending answer %s"%answer
            self.connection.sendall("%5.5d"%len(answer)+answer)
        else:
            print "Answer is too long: cannot send"

    def get_board_config_daq(self,brdid):
        if brdid == self.run.calib_board:
            return self.run.adcboard_list[0].format_config_daq()
        else:
            return "ERROR: board id %d currently not active"%brdid

    def get_board_config_zsup(self,brdid):
        if brdid == self.run.calib_board:
            return self.run.adcboard_list[0].format_config_zsup()
        else:
            return "ERROR: board id %d currently not active"%brdid

    def get_board_log_file_daq(self,brdid):
        if brdid == self.run.calib_board:
            return self.run.adcboard_list[0].log_file_daq
        else:
            return "ERROR: board id %d currently not active"%brdid

    def get_board_log_file_zsup(self,brdid):
        if brdid == self.run.calib_board:
            return self.run.adcboard_list[0].log_file_zsup
        else:
            return "ERROR: board id %d currently not active"%brdid

    def get_trig_config(self):
        return self.run.trigger.format_config()

    def get_trig_log(self):
        return self.run.trigger.log_file

    def change_crystal(self,board,channel):

        # Check if crystal address (board/channel) is correct
        if board<0 or (board>9 and board<14) or board>23 or channel<0 or channel>31:
            print "change_crystal - ERROR: request to set out of bound board/channel: %d/%d"%(board,channel)
            return "error"

        print "change_crystal - changing crystal to board/channel %d/%d"%(board,channel)
        self.run.calib_board = board
        self.run.calib_channel = channel
        if self.run.change_setup(self.calib_setup) == "error":
            print "CalibRunControl::change_crystal - ERROR while setting board/channel to %d/%d"%(board,channel)
            return "error"

        return "%d,%d"%(board,channel)

    def change_hv(self,hv):

        print "change_hv - changing HV to %d"%hv
        self.run.calib_hv = hv
        return "%d"%hv

    def new_run(self):

        newrun_number  = 0
        newrun_type    = "CALIBRATION"
        newrun_user    = "ECal Calibration"
        newrun_comment = "Board %d Channel %d HV %d"%(self.run.calib_board,self.run.calib_channel,self.run.calib_hv)

        print "Run number:  %d"%newrun_number
        print "Run type:    %s"%newrun_type
        print "Run crew:    %s"%newrun_user
        print "Run comment: %s"%newrun_comment

        # Set run configuration according to user's request
        self.run.run_number        = newrun_number
        self.run.run_type          = newrun_type
        self.run.run_user          = newrun_user
        self.run.run_comment_start = newrun_comment
        if not self.run.change_run():
            self.send_answer("init_error")
            return "error"

        # Create directory to host log files
        print "Creating log directory %s"%self.run.log_dir
        self.run.create_log_dir()

        # Write configuration files
        print "Writing configuration files for run %d"%self.run.run_number
        self.run.write_config()

        # Create merger input and output lists
        self.run.create_merger_input_list()
        self.run.create_merger_output_list()

        # Start run initialization procedure
        self.send_answer("init_start")

        ## Create level1 output rawdata directories
        self.run.create_level1_output_dirs()

        # Create pipes for data transfer
        print "Creating named pipes for run %d"%self.run.run_number
        self.run.create_fifos()

        # Start Level1 processes
        for lvl1 in (self.run.level1_list):
            p_id = lvl1.start_level1()
            if p_id:
                print "Level1 %d - Started with process id %d"%(lvl1.level1_id,p_id)
                self.send_answer("level1 %d ready"%lvl1.level1_id)
            else:
                print "Level1 %d - ERROR: could not start process"%lvl1.level1_id
                self.send_answer("level1 %d fail"%lvl1.level1_id)
            time.sleep(0.5)

        # Create receiving end of network tunnels (if needed)
        self.run.create_receivers()

        # Start merger
        p_id = self.run.merger.start_merger()
        if p_id:
            print "Merger - Started with process id %d"%p_id
            self.send_answer("merger ready")
        else:
            print "Merger - ERROR: could not start process"
            self.send_answer("merger fail")

        # Create sending ends of network tunnels (if needed)
        self.run.create_senders()

        # Start trigger process
        p_id = self.run.trigger.start_trig()
        if p_id:
            print "Trigger - Started with process id %d"%p_id
            self.send_answer("trigger init")
        else:
            print "Trigger - ERROR: could not start process"
            self.send_answer("trigger fail")
            self.send_answer("init_fail")
            return "initfail"

        # Start ZSUP for all boards
        for adc in (self.run.adcboard_list):

            p_id = adc.start_zsup()
            if p_id:
                print "ADC board %02d - Started ZSUP with process id %d"%(adc.board_id,p_id)
                self.send_answer("adc %d zsup_init"%adc.board_id)
            else:
                print "ADC board %02d - ERROR: could not start ZSUP"%adc.board_id
                self.send_answer("adc %d zsup_fail"%adc.board_id)
            time.sleep(0.5)

        # Start DAQ for all boards
        for adc in (self.run.adcboard_list):

            p_id = adc.start_daq()
            if p_id:
                print "ADC board %02d - Started DAQ with process id %d"%(adc.board_id,p_id)
                self.send_answer("adc %d daq_init"%adc.board_id)
                adc.status = "init"
            else:
                print "ADC board %02d - ERROR: could not start DAQ"%adc.board_id
                self.send_answer("adc %d daq_fail"%adc.board_id)
                adc.status = "fail"
            time.sleep(0.5)

        # Wait for trigger to complete initialization
        n_try = 1
        while(True):
            trig_status = self.check_trig_init_status(self.run.trigger)
            if (trig_status == "ready"):
                print "Trigger board - Initialized and ready for DAQ"
                self.send_answer("trigger ready")
                break
            elif (trig_status == "fail"):
                print "*** ERROR *** Trigger board failed the initialization. Cannot start run"
                if (self.run.run_number):
                    self.db.set_run_status(self.run.run_number,self.db.DB_RUN_STATUS_INIT_ERROR)
                self.send_answer("init_fail")
                return "initfail"
            n_try += 1
            if (n_try>=60):
                print "*** ERROR *** Trigger board did not initialize within 30sec. Cannot start run"
                if (self.run.run_number):
                    self.db.set_run_status(self.run.run_number,self.db.DB_RUN_STATUS_INIT_ERROR)
                self.send_answer("init_timeout")
                return "initfail"
            time.sleep(0.5)

        # Wait for all boards to complete initialization
        n_try = 0
        while(1):

            all_boards_init = True
            all_boards_ready = True
            for adc in (self.run.adcboard_list):

                # Check if any board changed status
                if (adc.status == "init"):

                    adc.status = self.check_init_status(adc)
                    if (adc.status == "ready"):
                        # Initialization ended OK
                        print "ADC board %02d - Initialized and ready for DAQ"%adc.board_id
                        self.send_answer("adc %d ready"%adc.board_id)
                    elif (adc.status == "fail"):
                        # Problem during initialization
                        print "ADC board %02d - *** Initialization failed ***"%adc.board_id
                        self.send_answer("adc %d fail"%adc.board_id)
                    else:
                        # This board is still initializing
                        all_boards_init = False

                # Check if any board is in fail status
                if (adc.status == "fail"): all_boards_ready = False

            # Check if all boards completed initialization
            if (all_boards_init):

                # Check if all boards initialized correctly
                if (all_boards_ready):

                    print "ADC boards - All boards initialized and ready for DAQ"
                    self.send_answer("adc all ready")
                    break

                else:

                    print "*** ERROR *** One or more boards failed the initialization. Cannot start run"
                    if (self.run.run_number):
                        self.db.set_run_status(self.run.run_number,self.db.DB_RUN_STATUS_INIT_ERROR)
                    self.send_answer("init_fail")
                    return "initfail"

            # Some boards are still initializing: keep waiting (wait up to ~30sec)
            n_try += 1
            if (n_try>=60):
                print "*** ERROR *** One or more boards did not initialize within 30sec. Cannot start run"
                if (self.run.run_number):
                    self.db.set_run_status(self.run.run_number,self.db.DB_RUN_STATUS_INIT_ERROR)
                self.send_answer("init_timeout")
                return "initfail"
            time.sleep(0.5)

        # All subsystems initialized: ready to start the run
        print "RunControl - All subsystems initialized: DAQ run can be started"
        if (self.run.run_number):
            self.db.set_run_time_init(self.run.run_number,self.db.now_str())
            self.db.set_run_status(self.run.run_number,self.db.DB_RUN_STATUS_INITIALIZED)
        self.send_answer("init_ready")
        return "initialized"

    def start_run(self):

        print "Starting run"

        self.run.start()

        self.send_answer("run_started")

        # RunControl is now in "running" mode
        return "running"

    def stop_run(self):

        self.run.run_comment_end = "EoR"
        print "End of Run comment: %s"%self.run.run_comment_end

        print "Stopping run"

        return self.terminate_run()

    def abort_run(self):


        print "Aborting run"
        self.run.run_comment_end = "Run aborted"

        return self.terminate_run()

    def terminate_run(self):

        self.send_answer("terminate_start")

        self.run.stop()

        terminate_ok = True

        # Run stop_daq procedure for each ADC board
        for adc in (self.run.adcboard_list):
            if adc.stop_daq():
                self.send_answer("adc %d daq_terminate_ok"%adc.board_id)
                print "ADC board %02d - DAQ terminated correctly"%adc.board_id
            else:
                terminate_ok = False
                self.send_answer("adc %d daq_terminate_error"%adc.board_id)
                print "ADC board %02d - WARNING: problems while terminating DAQ"%adc.board_id

        # Run stop_zsup procedure for each ADC board
        for adc in (self.run.adcboard_list):
            if adc.stop_zsup():
                self.send_answer("adc %d zsup_terminate_ok"%adc.board_id)
                print "ADC board %02d - ZSUP terminated correctly"%adc.board_id
            else:
                terminate_ok = False
                self.send_answer("adc %d zsup_terminate_error"%adc.board_id)
                print "ADC board %02d - WARNING: problems while terminating ZSUP"%adc.board_id

        # Run stop_trig procedure
        if self.run.trigger.stop_trig():
            self.send_answer("trigger terminate_ok")
            print "Trigger terminated correctly"
        else:
            terminate_ok = False
            self.send_answer("trigger terminate_error")
            print "WARNING: problems while terminating Trigger"

        # Run stop_merger procedure
        if self.run.merger.stop_merger():
            self.send_answer("merger terminate_ok")
            print "Merger terminated correctly"
        else:
            terminate_ok = False
            self.send_answer("merger terminate_error")
            print "WARNING: problems while terminating Merger"

        # Run stop_level1 procedures
        for lvl1 in self.run.level1_list:
            if lvl1.stop_level1():
                self.send_answer("level1 %d terminate_ok"%lvl1.level1_id)
                print "Level1 %02d terminated correctly"%lvl1.level1_id
            else:
                terminate_ok = False
                self.send_answer("level1 %d terminate_error"%lvl1.level1_id)
                print "Level1 %02d - WARNING: problems while terminating"%lvl1.level1_id

        # Clean up run directory
        self.run.clean_up()

        if terminate_ok:
            self.send_answer("terminate_ok")
        else:
            self.send_answer("terminate_error")

        # At the end of this procedure RunControl is back to "idle" mode
        return "idle"

    def check_init_status(self,adc):

        if adc.node_id == 0:

            if (os.path.exists(adc.initok_file_daq) and os.path.exists(adc.initok_file_zsup)):
                return "ready"
            elif (os.path.exists(adc.initfail_file_daq) or os.path.exists(adc.initfail_file_zsup)):
                return "fail"
            else:
                return "init"

        else:

            if (self.file_exists(adc.node_ip,adc.initok_file_daq) and self.file_exists(adc.node_ip,adc.initok_file_zsup)):
                return "ready"
            elif (self.file_exists(adc.node_ip,adc.initfail_file_daq) or self.file_exists(adc.node_ip,adc.initfail_file_zsup)):
                return "fail"
            else:
                return "init"

    def check_trig_init_status(self,trig):

        if trig.node_id == 0:

            if (os.path.exists(trig.initok_file)):
                return "ready"
            elif (os.path.exists(trig.initfail_file)):
                return "fail"
            else:
                return "init"

        else:

            if (self.file_exists(trig.node_ip,trig.initok_file)):
                return "ready"
            elif (self.file_exists(trig.node_ip,trig.initfail_file)):
                return "fail"
            else:
                return "init"

    def file_exists(self,node_ip,name):

        command = "ssh -i %s %s '( test -e %s )'"%(self.ssh_id_file,node_ip,name)
        rc = os.system(command)
        if (rc == 0): return True
        return False

def print_help():
    print 'CalibRunControl [-i|--interactive] [-h|--help]'
    print '   Start server to handle ECal calibration DAQ processes.'
    print '   -i|--interactive       Run in interactive mode. Default: run as a daemon.'
    print '   -h|--help              Show this help message and exit.'

def main(argv):

    try:
        opts,args = getopt.getopt(argv,"hi",["help","interactive"])
    except getopt.GetoptError:
        print_help()
        sys.exit(2)

    serverInteractive = False
    for opt,arg in opts:
        if opt == '-h' or opt == '--help':
            print_help()
            sys.exit()
        elif opt == '-i' or opt == '--interactive':
            serverInteractive = True

    # Just start the RunControlServer daemon
    if serverInteractive:
        RunControlServer("i")
    else:
        print "Starting RunControlServer in background"
        with daemon.DaemonContext(working_directory="."): RunControlServer("d")

# Execution starts here
if __name__ == "__main__":
   main(sys.argv[1:])
