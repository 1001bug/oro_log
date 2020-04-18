#!/bin/bash 
if [ $UID != 0 ]; then
	echo ROOT!
	exit -1
fi
#run as root (sudo)
#Switch turbo off for measure stability
echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo
grep -H -E '' /sys/devices/system/cpu/intel_pstate/no_turbo
echo -1 > /proc/sys/kernel/perf_event_paranoid
grep -H -E '' /proc/sys/kernel/perf_event_paranoid
