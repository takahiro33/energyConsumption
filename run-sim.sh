#!/bin/bash

WAFDIR=./
WAF=${WAFDIR}waf

mkdir results/
mkdir results/graphs/

for  csSize in 0 64 128 256
do
    for consumer in 4 6 10 15 20 
    do
	for intFreq in 50 150 300
	do	   
	    echo "run $WAF --run \"campus --intFreq=$intFreq --contents=100000 --css=$csSize  --LAN=$consumer\""
	    $WAF --run "campus --intFreq=$intFreq --contents=100 --css=$csSize --LAN=$consumer"
	done
    done		
done
