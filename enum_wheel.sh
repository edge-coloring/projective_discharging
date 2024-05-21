#!/bin/bash

#
# The script is used to enumerate wheels.
# We specify the degree of the hub and the directory that contains configuration files.
# The log file (e.g. proj_deg7.log, ...) is placed in ./log directory. 
# The results (the wheel files) will be placed in ./proj_wheel directory.
#
# Usage)
# bash enum_wheel.sh proj <The degree of the hub> <The directory that contains configurations>
#
# Example)
# bash enum_wheel.sh proj 7 projective_configurations/reducible/conf
# 

set -euxo pipefail
cd $(dirname $0)

if [ $# -ne 3 ]; then
    echo -e "\e[31merror:\e[m Please follow the usage 'bash enum_wheel.sh proj <The degree of the hub> <The directory that contains configurations>'"
    exit 1
fi

deg=$2
conf=$3
send="./proj_send"
wheel="./proj_wheel"

mkdir -p log
mkdir -p "$wheel"

if [ "$1" = "proj" ]; then
    ./build/a.out -d "$deg" -c "$conf" -s "$send" -m 9 -o "$wheel" -v 1 > ./log/proj_deg$deg.log &
fi