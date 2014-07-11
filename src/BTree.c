#include "BTree.h"

static void DisposeBTreeNode(BTree * bTree,BTree ** parent,BTree ** child)
{
    if(parent != NULL)
    {
        *parent = bTree->Parent;
    }

    if(child != NULL)
    {
        *child = bTree->Child;
    }

    free(bTree->Children);
    free(bTree->Keys);
    free(bTree);
}


static int SerializeBTreeNode(BTree * bTree,int order,int fd)
{
    off_t offset;
    if(bTree->DiskInfo.offset == -1)
    {
        offset = lseek(fd,0,SEEK_END);
    }
    else
    {
        offset = lseek(fd,bTree->DiskInfo.offset,SEEK_SET);
    }

    write(fd,&(bTree->NumberOfKeys),sizeof(int));
    write(fd,&(bTree->NumberOfChildren),sizeof(long));
    write(fd,bTree->Keys,sizeof(int) * (order - 1));
    write(fd,bTree->Children,sizeof(long) * order);

    bTree->isDirty = 0;
    bTree->DiskInfo.offset = offset;

    return 1;
}

static int DeserializeBTreeNode(BTree ** bTree,int order,int fd,long offset)
{
    if(CreateBTreeNode(order,bTree) == -1)
    {
        return -1;
    }

    lseek(fd,offset,SEEK_SET);

    if(read(fd,&((*bTree)->NumberOfKeys),sizeof(int)) != sizeof(int))
    {
        return -1;
    }
    if(read(fd,&((*bTree)->NumberOfChildren),sizeof(long)) != sizeof(long))
    {
        return -1;
    }
    if(read(fd,(*bTree)->Keys,sizeof(int) * (order - 1)) != (sizeof(int) * (order - 1)))
    {
        return -1;
    }
    if(read(fd,(*bTree)->Children,sizeof(long) * order) != (sizeof(long) * order))
    {
        return -1;
    }

    (*bTree)->isDirty = 0;
    (*bTree)->Child = (*bTree)->Parent = NULL;
    (*bTree)->DiskInfo.offset = offset;

    return 1;
}

static void DisposeBTreeLinkedList(BTree * bTreeLinkedList)
{
    while(bTreeLinkedList != NULL)
    {
        DisposeBTreeNode(bTreeLinkedList,NULL,&bTreeLinkedList);
    }
}



//Offset will be in +1 pos. than the key insertion position
static int SplitBTree(BTree * bTree,BTree ** splitedBTree,int order,long offsetWantedToInsert,int keyWantedToInsert,int * keyToBePromoted)
{
    int * keyArray = (int *) malloc(sizeof(int) * (bTree->NumberOfKeys + 1));
    int * offsetArray = NULL;
    if(bTree->NumberOfChildren != 0)
    {
        offsetArray = (int *) malloc(sizeof(long) * (bTree->NumberOfChildren + 1));
    }

    memcpy(keyArray,bTree->Keys,sizeof(int) * bTree->NumberOfKeys);
    int keyPosition = InsertKeyIntoArray(keyArray,bTree->NumberOfKeys,keyWantedToInsert);

    if(bTree->NumberOfChildren != 0)
    {
        memcpy(offsetArray,bTree->Children,sizeof(long) * bTree->NumberOfChildren);
        InsertOffSetIntoArray(offsetArray,bTree->NumberOfChildren,keyPosition + 1,offsetWantedToInsert);
    }

    int keyElementToBePromoted = ceil((double)(bTree->NumberOfKeys) / 2);

    *keyToBePromoted = keyArray[keyElementToBePromoted];

    CreateBTreeNode(order,splitedBTree);

    int numberOfKeysOriginally = bTree->NumberOfKeys;

    int numberOfChildrenOrginally = bTree->NumberOfChildren;

    ReWriteKeyOfBTreeNode(bTree,order,keyArray,keyElementToBePromoted);

    ReWriteKeyOfBTreeNode(*splitedBTree,order,keyArray + keyElementToBePromoted + 1,
                          (numberOfKeysOriginally + 1) - (keyElementToBePromoted + 1));

    if(bTree->NumberOfChildren != 0)
    {
        ReWriteOffsetOfBTreeNode(bTree,order,offsetArray,keyElementToBePromoted + 1);

        ReWriteOffsetOfBTreeNode(*splitedBTree,order,offsetArray + keyElementToBePromoted + 1,
                                 (numberOfChildrenOrginally + 1) - (keyElementToBePromoted + 1));
    }

    return 1;

}

static int ReWriteOffsetOfBTreeNode(BTree * bTree,int order,long * array,int length)
{
    if(length > order)
    {
        return -1;
    }
    int i;
    for(i = 0 ; i < length ; i++)
    {
        bTree->Children[i] = array[i];
    }
    bTree->NumberOfChildren = length;
    bTree->isDirty = 1;
    return 1;
}


static int ReWriteKeyOfBTreeNode(BTree * bTree,int order,int * array,int length)
{
    if(length > order - 1)
    {
        return -1;
    }
    int i;
    for(i = 0 ; i < length ; i++)
    {
        bTree->Keys[i] = array[i];
    }
    bTree->NumberOfKeys = length;
    bTree->isDirty = 1;
    return 1;
}

static int InsertOffSetIntoArray(long *array,int length,int position,long offset)
{
    int j;
    for(j = length - 1 ; j >= position ; j--)
    {
        array[j + 1] = array[j];
    }
    array[position] = offset;
    return position;
}

static int InsertKeyIntoArray(int * array,int length,int key)
{
    int i;
    for(i = 0 ; i < length ; i++)
    {
        if(array[i] >= key)
        {
            break;
        }
    }
    int j;
    for(j = length - 1 ; j >= i ; j--)
    {
        array[j + 1] = array[j];
    }

    array[i] = key;

    return i;
}

static int InsertKeyToBTreeNode(BTree * bTree,int order,int key,long offset)
{
    if(bTree->NumberOfKeys == order - 1)
    {
        return -1;
    }
    int position = InsertKeyIntoArray(bTree->Keys,bTree->NumberOfKeys,key);
    bTree->NumberOfKeys++;
    bTree->isDirty = 1;
    if(bTree->NumberOfChildren != 0)
    {
        InsertOffSetIntoArray(bTree->Children,bTree->NumberOfChildren,position + 1,offset);
        bTree->NumberOfChildren++;
    }
    return 1;
}


static int CreateBTreeNode(int order,BTree ** bTree)
{
    *bTree = (BTree *) malloc(sizeof(BTree));

    if((*bTree) == NULL)
    {
        return -1;
    }

    (*bTree) -> NumberOfChildren = 0;
    (*bTree) -> NumberOfKeys = 0;

    (*bTree) -> Keys = calloc(order - 1,sizeof(int));
    (*bTree) -> Children = calloc(order,sizeof(long));

    if((*bTree) -> Keys  == NULL || (*bTree) -> Children == NULL)
    {
        return -1;
    }

    (*bTree)->Child = (*bTree)->Parent = NULL;
    (*bTree)->DiskInfo.offset = -1;
    (*bTree)->isDirty = 1;

    return 1;
}

int FindKey(long rootOffSet,int fd,int keyFind,int keepVisitedNodeRecord,int order,BTree ** bTreeVisitedLinkedList,BTree ** bTreeVisitedLinkedListTail)
{
    int tailReached = 0;

    BTree * visitedLinkedList = NULL;

    BTree * visitedLinkedListTail = NULL;

    BTree * currentLinkedListNode = NULL;

    if(DeserializeBTreeNode(&visitedLinkedList,order,fd,rootOffSet) == -1)
    {
        return -1;
    }

    currentLinkedListNode = visitedLinkedList;

    do
    {
        visitedLinkedListTail = currentLinkedListNode;
        if(currentLinkedListNode->NumberOfKeys == 0)
        {
            tailReached = 1;
            break;
        }
        int i = 0;
        while(i <= currentLinkedListNode->NumberOfKeys)
        {
            if(i == currentLinkedListNode->NumberOfKeys || keyFind < currentLinkedListNode->Keys[i])
            {
                if(currentLinkedListNode->NumberOfChildren == 0)
                {
                    currentLinkedListNode = NULL;
                    break;
                }
                if(DeserializeBTreeNode(&(currentLinkedListNode->Child),order,fd,currentLinkedListNode->Children[i]) == -1)
                {
                    return -1;
                }
                currentLinkedListNode->Child->Parent = currentLinkedListNode;
                currentLinkedListNode = currentLinkedListNode->Child;
                break;
            }
            else if(currentLinkedListNode->Keys[i] == keyFind)
            {
                return 1;
            }
            else
            {
                i++;
            }
        }
    }while(tailReached == 0 && currentLinkedListNode != NULL);

    if(keepVisitedNodeRecord == 1)
    {
        *bTreeVisitedLinkedList = visitedLinkedList;
        *bTreeVisitedLinkedListTail = visitedLinkedListTail;
    }
    else
    {
        DisposeBTreeLinkedList(visitedLinkedList);
    }

    return 0;

}

long CreateRootNode(int order,int fd,int isSplitingOldRootNode,int keyToInsert,long offsetOfOldRootNode,long offsetOfNewSplittedNode)
{
    long offsetOfRootNode = -1;

    BTree * rootNode;
    CreateBTreeNode(order,&rootNode);

    if(isSplitingOldRootNode == 0)
    {
        write(fd,&offsetOfRootNode,sizeof(long));
        SerializeBTreeNode(rootNode,order,fd);
        offsetOfRootNode = rootNode->DiskInfo.offset;
        DisposeBTreeNode(rootNode,NULL,NULL);
    }
    else
    {
        rootNode->Keys[0] = keyToInsert;
        rootNode->NumberOfKeys++;

        rootNode->Children[0] = offsetOfOldRootNode;
        rootNode->Children[1] = offsetOfNewSplittedNode;
        rootNode->NumberOfChildren += 2;

        rootNode->isDirty = 1;
        SerializeBTreeNode(rootNode,order,fd);
        offsetOfRootNode = rootNode->DiskInfo.offset;
        DisposeBTreeNode(rootNode,NULL,NULL);
    }
    lseek(fd,0,SEEK_SET);
    write(fd,&offsetOfRootNode,sizeof(long));
    return offsetOfRootNode;
}

static void InitializeQueue(Queue ** queue)
{
    *queue = (Queue *) malloc(sizeof(Queue));
    (*queue)->CurrentFront = (*queue)->CurrentRear = 0;
    (*queue)->NumberOfElementInQueue = 0;
    (*queue)->QueueStorage = (long *) malloc(sizeof(long) * 1024);
    (*queue)->StorageLength = 1024;
}


static long DequeueElement(Queue * queue)
{
    if(queue->CurrentFront == queue->CurrentRear)
    {
        return -1;
    }

    long currentRear = queue->QueueStorage[queue->CurrentRear];
    queue->CurrentRear = queue->CurrentRear + 1;
    if(queue->CurrentRear >= queue->StorageLength)
    {
        queue->CurrentRear = 0;
    }
    return currentRear;
}

static int IsEmpty(Queue * queue)
{
    if(queue->CurrentFront == queue->CurrentRear)
    {
        return 1;
    }
    return 0;
}

static int EnqueueElement(Queue * queue,long element)
{
    if(queue->CurrentFront + 1 == queue->CurrentRear
       || (queue->CurrentFront == (queue->StorageLength - 1) && queue->CurrentRear == 0))
    {
        return -1;
    }

    queue->QueueStorage[queue->CurrentFront] = element;
    queue->CurrentFront = queue->CurrentFront + 1;
    if(queue->CurrentFront >= queue->StorageLength)
    {
        queue->CurrentFront = 0;
    }

    return 1;
}

static void ResetQueue(Queue * queue)
{
    queue->CurrentFront = queue->CurrentRear = 0;
    queue->NumberOfElementInQueue = 0;
}

int PrintBTree(int fd,int order,long rootOffSet,Queue ** queue)
{
    if(*queue == NULL)
    {
        InitializeQueue(queue);
    }
    else
    {
        ResetQueue(*queue);
    }

    BTree * currentNode = NULL;
    EnqueueElement(*queue,rootOffSet);
    EnqueueElement(*queue,0);
    int level = 1;
    while(IsEmpty(*queue) != 1)
    {
        long elementDequeued = DequeueElement(*queue);
        int i;
        if(elementDequeued != 0)
        {
            printf(" %d: ",level);
        }
        while(elementDequeued != 0)
        {
            DeserializeBTreeNode(&currentNode,order,fd,elementDequeued);
            for(i = 0; i < currentNode->NumberOfKeys - 1 ; i++)
            {
                printf("%d,",currentNode->Keys[i]);
            }
            printf("%d ",currentNode->Keys[currentNode->NumberOfKeys - 1]);
            for(i = 0 ; i < currentNode->NumberOfChildren; i++)
            {
                EnqueueElement(*queue,currentNode->Children[i]);
            }
            DisposeBTreeNode(currentNode,NULL,NULL);
            elementDequeued = DequeueElement(*queue);
        }
        if(i != 0)
        {
            EnqueueElement(*queue,0);
        }
        printf("\n");
        level++;
    }
    return 1;

}

int AddKey(int fd,long rootOffSet,int order,long * newRootOffSet,int * isRootOffSetChanged,int key)
{
    BTree * visitedBTreeLinkedList = NULL;
    BTree * visitedBTreeLinkedListTail = NULL;

    int findResult = FindKey(rootOffSet,fd,key,1,order,&visitedBTreeLinkedList,&visitedBTreeLinkedListTail);

    if(findResult == 1)
    {
        return 0;
    }
    if(findResult == -1)
    {
        return -1;
    }

    BTree * currentBTreeNodeToInspect = visitedBTreeLinkedListTail;

    long currentOffSet = -1;

    int currentKeyToPromote = key;

    BTree * splitedBTree;

    int isRootNodeReached = 0;

    while(1)
    {
        if(InsertKeyToBTreeNode(currentBTreeNodeToInspect,order,currentKeyToPromote,currentOffSet) == -1)
        {
            SplitBTree(currentBTreeNodeToInspect,&splitedBTree,order,currentOffSet,currentKeyToPromote,&currentKeyToPromote);
            SerializeBTreeNode(splitedBTree,order,fd);
            currentOffSet = splitedBTree->DiskInfo.offset;
            DisposeBTreeNode(splitedBTree,NULL,NULL);
            currentBTreeNodeToInspect = currentBTreeNodeToInspect->Parent;
            if(currentBTreeNodeToInspect == NULL)
            {
                isRootNodeReached = 1;
                break;
            }
        }
        else
        {
            break;
        }

    }

    if(isRootNodeReached)
    {
        *isRootOffSetChanged = 1;
        *newRootOffSet = CreateRootNode(order,fd,1,currentKeyToPromote,rootOffSet,currentOffSet);
    }

    currentBTreeNodeToInspect = visitedBTreeLinkedList;

    while(currentBTreeNodeToInspect != NULL)
    {
        if(currentBTreeNodeToInspect->isDirty == 1)
        {
            SerializeBTreeNode(currentBTreeNodeToInspect,order,fd);
        }
        currentBTreeNodeToInspect = currentBTreeNodeToInspect->Child;
    }

    DisposeBTreeLinkedList(visitedBTreeLinkedList);
    return 1;
}

