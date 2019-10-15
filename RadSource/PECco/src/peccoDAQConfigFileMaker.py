import os

daqFile = [
("executable","/home/daq/Padme/PadmeDAQ/PadmeDAQ.exe"),
("config_file","cfg/run_0/run_0_b00.cfg"),
("log_file","log/run_0/run_0_b00.log"),
("run_number","0"),
("board_id","0"),
("start_file","run/start"),
("quit_file","run/quit"),
("initok_file","run/initok_b00"),
("initfail_file","run/initfail_b00"),
("lock_file","run/lock_b00"),
("data_file","data/CrystalCheck+"),
("total_daq_time","120"),  # => per Clara's req / 10/04/18 
("startdaq_mode","0"),
("trigger_mode","1"),
("trigger_iolevel","NIM"),
("group_enable_mask","0xf"),
("channel_enable_mask","0x0000007f"),
("offset_global","0x6000"),
("post_trigger_size","50"),
("max_num_events_blt","128"),
("drs4corr_enable","1"),
("drs4_sampfreq","2"),
("daq_loop_delay","100000"),
("file_max_duration","6000"),
("file_max_size","1073741824"),
("file_max_events","50000000"),
("zero_suppression","0"),
]

DAQ_FILE_NAME ="CrystalTesting"

ChannelsMap = {
    #      DAQ chan, HV chan
   "00": (0,  0 ), 
   "01": (1,  1 ), 
   "02": (2,  2 ), 
   "03": (3,  3 ), 
   "04": (4,  4 ), 
   "10": (5,  5 ), 
   "11": (6,  6 ), 
   "12": (7,  7 ), 
   "13": (8,  8 ), 
   "14": (9,  9 ), 
   "20": (10, 10),  
   "21": (11, 11),  
   "22": (12, 12),  
   "23": (13, 13),  
   "24": (14, 14),  
   "30": (15, 15),  
   "31": (16, 16),  
   "32": (17, 17),  
   "33": (18, 18),  
   "34": (19, 19),  
   "40": (20, 20),  
   "41": (21, 21),  
   "42": (22, 22),  
   "43": (23, 23),  
   "44": (24, 24),  
}

def positionToChannel(position):
    pY, pX = [int(x) for x in position]
    return pX+5*pY

def mkDaqConfigDir(path):
    if os.path.isdir(path):
        #weird... it should not exist
        pass
    else:
        os.mkdir(path)
        os.mkdir("%s/cfg"%path)
        os.mkdir("%s/log"%path)
        os.mkdir("%s/run"%path)
        os.system("touch %s/run/start"%path)
        os.mkdir("%s/data"%path)

def mkDaqConfigFile(path, daqFileID, crystalPosition, **kvs):
    daqFileName ="cfg/%s_%s.cfg"%(DAQ_FILE_NAME, crystalPosition)
    of = open("%s/%s"%(path,daqFileName),"wt")
    for key, value in daqFile:
        if key in kvs:
            value = kvs[key]
        if key =="config_file":
            value ="%s_b%s.cfg"%(daqFileID, crystalPosition)
        elif key =="log_file":
            value ="%s_b%s.log"%(daqFileID, crystalPosition)
        elif key =="group_enable_mask":
            #value ="0x%d"%(1<<(ChannelsMap[crystalPosition][0]//8))
            value ="0x%x"%((1<<(ChannelsMap[crystalPosition][0]//8))|(1<<3)) # activate group for chan 25 for the LYSO
        elif key =="channel_enable_mask":
            #value ="0x%08x"%(1<<ChannelsMap[crystalPosition][0])
            value ="0x%08x"%((1<<ChannelsMap[crystalPosition][0])|(1<<25)) # always activate chan 25 for the LYSO
        of.write("%-24s %s\n"%(key,value))        
    of.close()
    return daqFileName
