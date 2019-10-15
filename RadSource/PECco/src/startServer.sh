#!/bin/bash

touch /home/daq/PadmeCrystalCheck/data/pcc_server.stderr.log
python /home/daq/PadmeCrystalCheck/src/pccServer.py 2>> /home/daq/PadmeCrystalCheck/data/pcc_server.stderr.log
