#!/bin/bash

export DMALLOC_OPTIONS="debug=0x4f46d03,log=./tt-dmalloc.log"

./src/tt -D -d -c /etc/jabber/tt.xml -u jabber
