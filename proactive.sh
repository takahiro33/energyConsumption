#!/bin/bash

WAFDIR=./
WAF=${WAFDIR}waf

mkdir results/
mkdir results/graphs/
mkdir results/normal/
mkdir results/proactive/

for  speed in  0 2 4 6 8 10
do

mkdir results/normal/speed:$speed/
mkdir results/proactive/speed:$speed/


for mobile in 1 2 5 
do

mkdir results/normal/speed:$speed/MN:$mobile
mkdir results/proactive/speed:$speed/MN:$mobile

# Creat Directries for trace files
echo "run $WAF --run \"icc-scenario_zl -speed=$speed -smart -mobile=$mobile -trace\""
$WAF --run "icc-scenario -speed=$speed -smart -mobile=$mobile -trace"

echo "run $WAF --run \"icc-scenario_zl -speed=$speed -trace -mobile=$mobile -proactive\""
$WAF --run "icc-scenario -speed=$speed -proactive -mobile=$mobile -trace" 

done

done
		
