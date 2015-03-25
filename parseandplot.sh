#!/bin/bash
NUMALIZE=$HOME/Work/numalize
base=$(pwd)
for d in $(\ls)
do
    if [ -d $d ]
    then
    echo "Plotting $d"
    cd $NUMALIZE/plotgen
    ./plotter.sh $base/$d $d
    cd -
    fi
done

