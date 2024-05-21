#!/bin/bash

# The script is used to execute the discharging procedure.
# We specify the degree of the hub(=: d), the smaller index of the range (=: l), the larger index of the range(=: r), 
# the directory that contains rule files, the directory that contains configuration files.
# Then, the script executes the discharging procedure to ./proj_wheel/d_l.wheel, ./proj_wheel/d_{l+1}.wheel ... ./proj_wheel/d_r.wheel
# The log files (e.g. 7_0.wheel.log) are placed in ./proj_log directory.
#
# Usage)
# bash discharge.sh proj <The degree of the hub> <The smaller index of the range> <The larger index of the range> <The directory that contains rule files> <The directory that contains configuration files>
#
# Example)
# bash discharge.sh proj 7 0 1500 projective_configurations/rule projective_configurations/reducible/conf
#
#

set -euxo pipefail
cd $(dirname $0)

if [ $# -ne 6 ]; then
    echo -e "\e[31merror:\e[m Please follow the usage 'bash discharge.sh proj <The degree of the hub> <The smaller index of the range> <The larger index of the range> <The directory that contains rule files> <The directory that contains configuration files>'"
    exit 1
fi

l=$3
r=$4
rule=$5
conf=$6

#
# degree 7: we need to execute ./proj_wheel/7_{0..4703}.wheel 
# degree 8: we need to execute ./proj_wheel/8_{0..6530}.wheel 
# degree 9: we need to execute ./proj_wheel/9_{0..5435}.wheel 
# degree 10: we need to execute ./proj_wheel/10_{0..3079}.wheel 
# degree 11: we need to execute ./proj_wheel/11_{0..1036}.wheel
#
if [ "$1" = "proj" ]; then
    send="./proj_send"
    mkdir -p proj_log
    for i in $(seq $l $r); do
        a=$(grep "the ratio of overcharged cartwheel 0/" ./proj_log/$2_$i.wheel.log | wc -l) && true
        if [ $a -eq 0 ]; then
            ./build/a.out -w "./proj_wheel/$2_$i.wheel" -r "$rule" -c "$conf" -s "$send" -m 9 -v 1 > "./proj_log/$2_$i.wheel.log" &
        fi
    done
fi

