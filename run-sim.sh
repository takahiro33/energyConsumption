#!/bin/bash

WAFDIR=./
WAF=${WAFDIR}waf

mkdir results/
mkdir results/graphs/
mkdir results/ip/
mkdir results/ccn/

for  csSize in 0 64 128 256
do

mkdir results/ip/csSize:$csSize/
mkdir results/ccn/csSize:$csSize/


for mobile in 1 6 12 18
do

mkdir results/ip/csSize:$csSize/MN:$mobile
mkdir results/ccn/csSize:$csSize/MN:$mobile

# Creat Directries for trace files
echo "run $WAF --run \"fixed -csSize=$csSize -smart -mobile=$mobile -trace\""
$WAF --run "fixed -csSize=$csSize -smart -mobile=$mobile -trace"

echo "run $WAF --run \"fixed -csSize=$csSize -trace -mobile=$mobile -smart\""
$WAF --run "fixed -csSize=$csSize -smart -mobile=$mobile -trace" 

done

done
		
