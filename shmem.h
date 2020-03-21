#ifndef SHMEM_H
#define SHMEM_H

typedef struct
{
    unsigned int secs;
    unsigned int nans;
} simclock;

typedef struct
{
    int pclass;
    int pids;
    int fpid;
    int priority;
    simclock smcputime;
    simclock smsystime;
    simclock smblktime;
    simclock smwaittime;
} pcblock;

typedef struct
{
    pcblock pctable[18];
    simclock simtime; 
} shmem;

#endif /* SHMEM_H */