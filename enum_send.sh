#!/bin/bash

#
# The script is used to enumerate the cases where a vertex sends a charge. 
# We specify the directory that contains rule files and the directory that contains configuration files.
# The log files (e.g. proj_from5_7+m9.log, ...) will be placed in ./log directory. 
# The results (the rule files that represents the cases where a vertex sends a charge) will be placed in ./proj_send directory.
#
# Usage)
# bash enum_send.sh proj <The directory that contains rule files> <The directory that contains configuration files>
#
# Example)
# bash enum_send.sh proj projective_configurations/rule projective_configurations/reducible/conf
#

set -euxo pipefail
cd $(dirname $0)

if [ $# -ne 3 ]; then
    echo -e "\e[31merror:\e[m Please follow the usage 'bash enum_send.sh proj <The directory that contains rules> <The directory that contains configurations>'"
    exit 1
fi

rule=$2
conf=$3
send="./proj_send"

mkdir -p log
mkdir -p "$send"

if [ "$1" = "proj" ]; then 
    ./build/send -f 5 -t 7+ -m 9 -r "$rule" -c "$conf" -o "$send" -v 1 > ./log/proj_from5_7+m9.log &
    ./build/send -f 6 -t 7+ -m 9 -r "$rule" -c "$conf" -o "$send" -v 1 > ./log/proj_from6_7+m9.log &
    ./build/send -f 7 -t 7+ -m 9 -r "$rule" -c "$conf" -o "$send" -v 1 > ./log/proj_from7_7+m9.log &
    ./build/send -f 8 -t 7+ -m 9 -r "$rule" -c "$conf" -o "$send" -v 1 > ./log/proj_from8_7+m9.log &
fi