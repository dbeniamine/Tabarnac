New visu:

# Intro

Classical issues

+ Remote stack access
    + First of all this is programatically bad
    + Can't be mapped efficiently by any tool
    + Use heap !!!
+ All to all sharing
    + Does not fit NUMA properties, try to make thread works on different part
    of the data
    + Replication if possible (no write, or merge in the end)
+ ??
outline of the next sections

## NUMA machine

Use lstopo plot

## Detected problems

Have we automatically detected a classical error

+   Yes:
    + what , where , how to fix it ?
    + Look at the plot one issue might hide and other one
+   No
        If you are using that program it probably means that you have some
        perf issues, please look at the plots of the following sections

# Sharing inside structures

For each struct the sharing by threads.
too much all to all sharing is not good for numa
The ideal thing is hierarchical sharing matching NUMA hierarchie

# By thread usage

+ Check memory balance
+ Check thread / struct affinity
