#!/bin/bash

WAFDIR=./
WAF=${WAFDIR}waf

mkdir results/
mkdir results/graphs/
mkdir results/ndn/
mkdir results/nnn/

for  speed in  2 4 6 8 10
do

mkdir results/ndn/speed:$speed/
mkdir results/nnn/speed:$speed/


for mobile in 1 2 5 
do

mkdir results/ndn/speed:$speed/MN:$mobile
mkdir results/nnn/speed:$speed/MN:$mobile

# Creat Directries for trace files
echo "run $WAF --run \"icc-scenario_zl -speed=$speed -smart -mobile=$mobile -trace\""
$WAF --run "icc-scenario_zl -speed=$speed -smart -mobile=$mobile -trace"

echo "run $WAF --run \"icc-scenario_zl -speed=$speed -trace -mobile=$mobile -smart\""
$WAF --run "icc-scenario_zl -speed=$speed -sinf -mobile=$mobile -trace" 

done

done
		
