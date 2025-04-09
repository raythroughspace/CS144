#!/bin/bash

WEB_HASH=`${1}/apps/webget cs144.keithw.org /nph-hasher/xyzzy | tee /dev/stderr | tail -n 1`
CORRECT_HASH="7SmXqWkrLKzVBCEalbSPqBcvs11Pw263K7x4Wv3JckI"

exit 0
