#!/bin/bash

WAFDIR=./
WAF=${WAFDIR}waf

mkdir results/
mkdir results/graphs/

for mode in "NormalTraffic"
do

mkdir results/$mode/

for  csSize in 0 64 128 256
do

mkdir results/$mode/csSize_$csSize/

for mobile in 12
do
mkdir results/$mode/csSize_$csSize/MN_$mobile/

for endTime in 500
do
mkdir results/$mode/csSize_$csSize/MN_$mobile/endTime_$endTime/


# Creat Directries for trace files
echo "run $WAF --run \"fixed -mode=\"$mode\" -csSize=$csSize -mobile=$mobile -servers=1 -contents=10000 -endTime=$endTime -trace\""
$WAF --run "fixed -mode="$mode" -csSize=$csSize -mobile=$mobile -servers=1 -contents=10000 -endTime=$endTime -trace"

done

done

done
		
done
