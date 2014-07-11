#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>


struct BTreeDiskInfo
{
    long offset;
};

typedef struct BTreeDiskInfo BTreeDiskInfo;

struct BTree
{
    int NumberOfKeys;
    int * Keys;
    long * Children;
    int NumberOfChildren;
    BTreeDiskInfo DiskInfo;
    struct BTree * Parent;
    struct BTree * Child;
    int isDirty;
};

struct Queue
{
    long * QueueStorage;
    int NumberOfElementInQueue;
    int CurrentFront;
    int CurrentRear;
    int StorageLength;
};

typedef struct Queue Queue;

typedef struct BTree BTree;
