#!/bin/sh

#scons --clean
scons -j16
./build/opt/zsim tests/graph500.cfg
