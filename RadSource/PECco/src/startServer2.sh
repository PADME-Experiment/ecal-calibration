#!/bin/bash

# find the correct path or use the CWD
export PECCO_SERVER="${PECCO_SERVER:-`pwd`}"

# init the stderr/stdout logfiles

touch ${PECCO_SERVER}/data/pecco_server.stderr.log
touch ${PECCO_SERVER}/data/pecco_server.stdout.log
nohup python ${PECCO_SERVER}/src/peccoServer.py 1>>${PECCO_SERVER}/data/pecco_server.stdout.log 2>>${PECCO_SERVER}/data/pecco_server.stderr.log &