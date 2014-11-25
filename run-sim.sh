#!/bin/bash

WAFDIR=./
WAF=${WAFDIR}waf

mkdir results/
mkdir results/graphs/
mkdir results/NormalTraffic/

for  csSize in 0 64 128 256
do


mkdir results/NormalTraffic/csSize_$csSize/


for mobile in 2 6 10 14
do

mkdir results/NormalTraffic/csSize_$csSize/MN_$mobile

# Creat Directries for trace files
echo "run $WAF --run \"wifi -csSize=$csSize -smart -mobile=$mobile -trace\""
$WAF --run "wifi -csSize=$csSize -smart -mobile=$mobile -trace"

done

done
		
