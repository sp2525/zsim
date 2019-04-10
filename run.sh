#!/bin/sh

scons --clean
scons -j16

DATE=`date +%Y%m%d_%H%M%S`
mkdir -p output/$DATE
rm -rf output/latest
ln -s $DATE output/latest

cd output/latest

time ../../build/opt/zsim ../../tests/graph500_tlb.cfg | tee run.log
#time ./build/opt/zsim tests/graph500_tlb.cfg | tee temp.log
#time ./build/opt/zsim tests/graph500.cfg | tee temp.log
