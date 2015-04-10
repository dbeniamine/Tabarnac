#Install dependencies, function from
# http://stackoverflow.com/questions/24294433/installing-and-loading-packages-with-a-single-function
# by OganM
insist  <-  function(name){
    #enables entering package name without quotes
    name = substitute(name) 
    name = as.character(name)

    if (!require(name, character.only = T, quiet=T)) {
        install.packages(name)
        Sys.sleep(2)
        library(name, character.only = T)
    }
}
insist(methods)
insist(knitr)
# Set variables
args <- commandArgs(TRUE)
path <- args[1]
name  <- args[2]
save  <-  args[3]
bw  <-  args[4]
output <- paste(path, '/', name, '-plots.html', sep='')
knit2html("plot.rmd", output)
