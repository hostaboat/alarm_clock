#!/usr/bin/env bash

if [ ! -d "build" ] ; then
    mkdir build
fi

xelatex -halt-on-error -output-directory build alarm_clock.tex > build/alarm_clock.output

if [[ "$?" == "0" ]] ; then
    cp build/alarm_clock.pdf .
    scp alarm_clock.pdf 192.168.0.102:Downloads
else
    tail build/alarm_clock.output
fi
