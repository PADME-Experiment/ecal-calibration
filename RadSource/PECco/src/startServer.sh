#!/bin/bash

# find the correct path or use the CWD
export PECCO_SERVER="${PECCO_SERVER:-`pwd`}"

# init the stderr logfile
touch ${PECCO_SERVER}/data/pecco_server.stderr.log
python ${PECCO_SERVER}src/peccoServer.py 2>> ${PECCO_SERVER}/data/pecco_server.stderr.log
