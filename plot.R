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

#Install dependencies, function from
# http://stackoverflow.com/questions/24294433/installing-and-loading-packages-with-a-single-function
# by OganM
insist  <-  function(name){
    #enables entering package name without quotes
    name = substitute(name) 
    name = as.character(name)

    if (!require(name, character.only = T, quiet=T)) {
        install.packages(name, verbose=T, repos="http://cran.rstudio.com/")
        Sys.sleep(2)
        library(name, character.only = T)
    }
}
insist(methods)
insist(knitr)
# Set variables
args   <- commandArgs(TRUE)
path   <- args[1]
name   <- args[2]
save   <-  args[3]
bw     <-  args[4]
titles <-  args[5]
imgsc  <-  as.numeric(as.character(args[6]))
output <- paste(path, '/', name, '-plots.html', sep='')
# Do the work
knit2html("plot.rmd", output)
