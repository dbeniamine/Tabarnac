#!/bin/bash

# Copyright (C) 2015  Beniamine, David <David@Beniamine.net>
# Author: Beniamine, David <David@Beniamine.net>
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

if [ -z "$1" ] || [ -z "$2" ]
then
    echo "Usage $0 exp-dir name [save]"
    echo "dir:     Path to the experiment files"
    echo "name:    Experiment name (common part of the experiment file names)"
    echo "save:    Save the plot image, true or false. default: false"
    echo "-h  Display this help and quit"
    exit 1
fi

usage()
{
    echo "Usage $0 -d dir -n name [options]"
    echo "dir:      Path to the experiment files"
    echo "name:     Experiment name (common part of the experiment file names)"
    echo "Options:"
    echo "-s        Save the plot image."
    echo "-b        Do black and white plots."
}


save=F
bw=F
while getopts "d:n:sbh" opt; do
    case $opt in
        d)
            dir="$OPTARG"
            ;;
        n)
            name="$OPTARG"
            ;;
        s)
            save="T"
            ;;
        b)
            bw="T"
            ;;
        h)
            usage
            exit 0
            ;;
        \?)
            echo "Invalid option : -$OPTARG" >&2
            usage
            exit 1
            ;;
    esac
done

set -x
if [ -z "$dir" ] || [ -z "$name" ]
then
    echo "dir and name args are required"
    usage
    exit 1
fi

Rscript metaplot.R $dir $name $save $bw
#"path <- '$1'; name <- '$2'; save <-'$save'; bw<-'$BW'; output='$1/$2-plots.html'; ./metaplot.R"
# Clean and copi figures
rm -rf $dir/figure
mv ./figure $dir/
