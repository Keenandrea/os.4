#ifndef SHMEM_H
#define SHMEM_H

typedef struct
{
    unsigned int secs;
    unsigned int nans;
} simclock;

typedef struct
{
    int processclass;
    int pids;
    int fpid;
} pcblock;

typedef struct
{
    pcblock pctable[18];
    simclock simtime; 
} shmem;

#endif /* SHMEM_H */