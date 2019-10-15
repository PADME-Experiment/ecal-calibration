#!/bin/bash

touch /home/daq/PadmeCrystalCheck/data/pcc_server.stderr.log
touch /home/daq/PadmeCrystalCheck/data/pcc_server.stdout.log
nohup python /home/daq/PadmeCrystalCheck/src/pccServer.py 1>>/home/daq/PadmeCrystalCheck/data/pcc_server.stdout.log 2>> /home/daq/PadmeCrystalCheck/data/pcc_server.stderr.log &
