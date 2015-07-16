# Tabarnac

## What is tabarnac

*TABARNAC*:  Tools for Analyzing the Behavior of Applications Running on NUMA
ArChitecture is a set of tools and methodologies to analyse performances
issues related to (NUMA) memory usage. Tabarnac consists on two parts:

1.  An efficient, lock-free instrumentation to trace the memory behavior at
    the granularity of the page (based on a modified version of
    [numalize](https://github.com/matthiasdiener/numalize)). The
    instrumentation tool is also able to retrieve data structure names, size
    and address by two means:

    * Binaries files are read using `libelfg0` to find statically allocated structures.
    * Mallocs are traced, and structure names are (heuristically) resolved using
      debug flags.

2.  A set of simple yet complete visualization providing a deep understanding
    of the memory usage.

The two part of the tool are completely independent therefore it is possible to
run and analyze on different machines (see [Advanced usage](#advanced-usage)).

The instrumentation is based on Intel's
[Pin](https://software.intel.com/en-us/articles/pintool) library. Although Pin
is designed for Intel processor, it's still work on AMD.

The visualization is generated by [R](http://cran.r-project.org/) and is able
to automatically install all the R libraries required.

## Installation

* Install the library gelf, `lstopo` and R (on Debian
 `apt-get install libelfg0  hwloc r-base`, the two first are required for
 instrumentation only and last is for visualization only).
* Install [Pin](https://software.intel.com/en-us/articles/pintool).
* Download Tabarnac
    git clone https://github.com/dbeniamine/Tabarnac.git

Everything should work out of the box !

## Usage

### Instrumentation

To instrument a program and generate the visualization, simply run:

    /path/to/tabarnac/tabarnac -- ./my_command my_arg0 my_arg1

It will generate 5 files and one directory:

    my_command.full.pages.csv
    my_command.structs.csv
    my_command.stackmap.csv
    my_command-plots.html
    advices.html
    figure/

The three first files are generated by the instrumentation and are not designed
to be read by an human, it gives (respectively) the number of access per page
and per threads, the addresses of the different data structures, the addresses
of each stack.

### Visualization

The file `my_commands-plots.html` contains the actual visualization of the
trace it consist on a set of plots coupled with explanation to ease their
reading.
<!--, an example trace is available [here](#Todo).-->

The first figure is the topology of the analysis machine, generated by
`lstopo`.

The two following visualization show the size and the number of reads and
writes for each data structure. They aim at understanding the relative
importance of each structure and give a small insight of which kind of
optimisation can be done for each structure (replication, splitting ...). A
tables also shows which structures are not read (or written) by which threads,
this is useful to determine if replication is a possible optimisation.

Then Tabarnac shows for each data structure the access distribution of each
thread. This visualization gives a deep understanding of the data sharing
between threads and allow the user to find a good NUMA data / thread mapping
or to decide to modify this pattern.

Finally as in most operating systems, memory pages are mapped near to the
first thread accessing them (first touch), we provide for each structure a
plot showing which thread is responsible of the first touch on each page. To
obtain the best performances, the first touch should pattern should be
correlated to the access distribution. If it is impossible, manual or
automatic data mapping should provide huge performances improvements.

## Advanced usage

It is often easier / preferable to install avoid installing to much things on
experimental machines, therefore Tabarnac allow you to run and generate the
visualisation on different machines.

### Run only


To only run the instrumentation on a machine run:

    /path/to/tabarnac/tabarnac -r -- ./my_command my_arg0 my_arg1

Save the `*.csv` and `topo.png`  to the machine on which you like to run the
visualization.

### Visualize only

To generate the visualization, got to the directory containing the files
previously generated and run:

    /path/to/tabarnac/tabarnac -p my_command

A few options are available to customize the plots:

    -i            Do not ignore smally used structures. This might
                            produce a hard to read output
    -b            Do black and white plots
    -s            Save plots (png files)
    -t            Disable titles in figures
    -S scale      Set the scale for saved figures implies -s
    -R ratio      set plots_width=ratio*plot_height, default=1

## Troubleshooting

### Pin installation

If pin is not installed in `/opt/pin`, you might got the following error:

    /bin/sh: 1: /opt/pin: not found

You can fix it either by setting `PIN_HOME` to the pin installation path

    export PIN_HOME=/path/to/pin

Or by directly giving the pin path to Tabarnac:

     /path/to/tabarnac/tabarnac -d /path/to/pin -- ./my_command my_arg0 my_arg1

### Debug flag not present warning

Although your application is compile with the `-g` flag, Tabarnac might show
the following warning:

    Can't open file '', malloc will be anonymous
    Have you compiled your program with '-g' flag ?

This happens when the application is linked to a library compiled without the
debug flags, it only means that structures allocated on the library won't be
analyzed.
