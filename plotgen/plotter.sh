#!/bin/bash
# TODO: use getopts
if [ -z "$1" ] || [ -z "$2" ]
then
    echo "Usage $0 exp-dir name [save]"
    echo "dir:     Path to the experiment files"
    echo "name:    Experiment name (common part of the experiment file names)"
    echo "save:    Save the plot image, true or false. default: false"
    exit 1
fi
save=F
BW=F
if [ ! -z "$3" ] && [ $3 == "true" ]
then
    save=T
fi

Rscript -e \
    "require(knitr); path <- '$1'; name <- '$2'; save <-'$save'; bw<-'$BW'; knit2html(\"plot.rmd\", output='$1/$2-plots.html')"
# Clean and copi figures
rm -rf $1/figure
mv ./figure $1/
