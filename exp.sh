#!/bin/bash
if [ -z "$1" ]
then
    echo "Usage $0 exp_name"
    exit 1
fi
NAS=$HOME/NAS-origin/bin
NUMALIZE=$HOME/numalize
EXP_DIR=$1
CMDLINE="$0 $@"
START_TIME=$(date +%y%m%d_%H%M%S)
OUTPUT=$EXP_DIR/exp.log
exec > >(tee $OUTPUT) 2>&1
dumpInfos
function testAndExitOnError
{
    err=$?
    if [ $err -ne 0 ]
    then
        echo "ERROR $err : $1"
        exit $err
    fi
}
function dumpInfos
{
    pwd
    #Echo start time
    echo "Expe started at $START_TIME"
    #Echo args
    echo "#### Cmd line args : ###"
    echo "$CMDLINE"
    echo "EXP_NAME $EXP_DIR"
    echo "OUTPUT: $OUTPUT"
    echo "########################"
    # DUMP environement important stuff
    echo "#### Hostname: #########"
    hostname
    echo "########################"
    echo "##### git log: #########"
    git log | head
    echo "########################"
    echo "#### git diff: #########"
    git diff
    echo "########################"
    lstopo --of txt
    cat /proc/cpuinfo
    echo "########################"


    #DUMPING scripts
    cp -v $0 $EXP_DIR/
    cp -v ./*.sh $EXP_DIR/
    cp -v *.pl $EXP_DIR/
    cp -v *.rmd  $EXP_DIR/
    cp -v Makefile  $EXP_DIR/
}
for f in $(\ls $NAS)
do
    cd $EXP_DIR
    mkdir $f
    cd $f
	echo $NUMALIZE/run.sh -p -- $NAS/$f
    testAndExitOnError "Benchmark $f"
    cd $NUMALIZE/plotgen
    ./plotter.sh $EXP_DIR/$f $f
done
echo "thermal_throttle infos :"
cat /sys/devices/system/cpu/cpu0/thermal_throttle/*
END_TIME=$(date +%y%m%d_%H%M%S)
echo "Expe ended at $END_TIME"
