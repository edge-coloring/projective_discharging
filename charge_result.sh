#!/bin/bash

#
# The script is used to make sure the hub of each wheel has charge at most 0.
# If the hub of all wheels has charge at most 0, the message "All finished" is displayed.
# 
# Usage)
# bash charge_result.sh proj <The degree of the hub> <The output file>
#
# Example)
# bash charge_result.sh proj 7 out_proj7.txt
# 

set -euxo pipefail
cd $(dirname $0)

if [ $# -ne 3 ]; then
    echo -e "\e[31merror:\e[m Please follow the usage 'bash charge_result.sh proj <The degree of the hub> <The output file>'"
    exit 1
fi


if [ "$1" = "proj" ]; then
    num_wheel=(0 0 0 0 0 0 0 4703 6530 5435 3079 1036)
    sum=0
    for i in $(seq 0 ${num_wheel["$2"]}); do
        a=$(grep 'the ratio of overcharged cartwheel 0/' "./proj_log/$2_$i.wheel.log" | wc -l) && true
        if [ $a -ne 1 ]; then
            echo "$i has overcharged cartwheel" >> "$3"
            sum=$(($sum+1))
        fi
    done
    if [ $sum -eq 0 ]; then
        echo "All finished!"
    else 
        echo "$sum cartwheel remained"
    fi
fi


