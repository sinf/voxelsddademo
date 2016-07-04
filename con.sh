#!/bin/sh
scons -j`cat /proc/cpuinfo|grep -c 'cpu family'` mode=debug $*
