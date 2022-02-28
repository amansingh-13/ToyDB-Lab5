#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include "tbl.h"
#include "codec.h"
#include "../pflayer/pf.h"

#define SLOT_COUNT_OFFSET 2
#define checkerr(err) {if (err < 0) {PF_PrintError(); exit(EXIT_FAILURE);}}

int getNumSlots(byte *pageBuf)
{
    return *((int*)pageBuf);
}
void setNumSlots(byte *pageBuf, int nslots)
{
    *((int*)pageBuf) = nslots;
}
int getNthSlotOffset(int slot, char* pageBuf)
{
    //                  base     no_of_recs  ptr_to_free_space
    //printf("%p PAGE BUFFER!!!!!!!\n", pageBuf);
    char** slot_list = (((byte*) pageBuf) + sizeof(int) + sizeof(char*));

    printf("%p, %p, %p\n", pageBuf, slot_list, slot_list[slot-1]);

    if(slot > 0)
        return slot_list[slot-1];
    else
        return PF_PAGE_SIZE;
}
int getLen(int slot, byte *pageBuf)
{
	int begin = getNthSlotOffset(slot,   (char*) pageBuf);
	int end   = getNthSlotOffset(slot-1, (char*) pageBuf);
    printf("b, e : %d, %d\n", begin, end);
    return end - begin;
}

void setNthSlotOffset(byte* pageBuf, int slot, char* ptr)
{
    //printf("%p PAGE BUFFER!!!!!!!\n", pageBuf);
    char** slot_list = (((byte*) pageBuf) + sizeof(int) + sizeof(char*));
    printf("1. %p, %p, %p\n", ptr, slot_list, slot_list[slot-1]);
    slot_list[slot-1] = ptr - pageBuf;
    printf("2. %p, %p, %p\n\n", ptr, slot_list, slot_list[slot-1]);
}
void setFreePointer(byte *pageBuf, char* ptr)
{
    *((char**)(pageBuf+sizeof(int))) = ptr;
}
char* getFreePointer(byte* pageBuf)
{
    return *((char**)(pageBuf+sizeof(int)));
}
int getFreeSpace(byte *pageBuf)
{
    return getFreePointer(pageBuf) - pageBuf - \
        (sizeof(int) + sizeof(char*) + getNumSlots(pageBuf)*sizeof(char*));
}


/**
   Opens a paged file, creating one if it doesn't exist, and optionally
   overwriting it.
   Returns 0 on success and a negative error code otherwise.
   If successful, it returns an initialized Table*.
 */
int
Table_Open(char *dbname, Schema *schema, bool overwrite, Table **ptable)
{
    // Initialize PF, create PF file,
    // allocate Table structure  and initialize and return via ptable
    // The Table structure only stores the schema. The current functionality
    // does not really need the schema, because we are only concentrating
    // on record storage.
    PF_Init();
    if(access(dbname, F_OK) != 0 || overwrite){
        PF_DestroyFile(dbname);
        PF_CreateFile(dbname);
    }
    int fd = PF_OpenFile(dbname);

    *ptable = (Table*) malloc(sizeof(Table));
    (*ptable)->fd = fd;
    (*ptable)->page_num = 0;
    (*ptable)->page_buf = NULL;

    return 0;
}

void
Table_Close(Table *tbl) 
{
    // Unfix any dirty pages, close file.
    if(tbl->page_buf != NULL){
        PF_UnfixPage(tbl->fd, tbl->page_num, TRUE);
    }
    PF_CloseFile(tbl->fd);
    free(tbl);
}


int
Table_Insert(Table *tbl, byte *record, int len, RecId *rid) 
{
    // Allocate a fresh page if len is not enough for remaining space
    // Get the next free slot on page, and copy record in the free
    // space
    // Update slot and free space index information on top of page.
    if(tbl->page_buf == NULL || getFreeSpace(tbl->page_buf) < len+sizeof(char*)){
        if(tbl->page_buf != NULL){
            PF_UnfixPage(tbl->fd, tbl->page_num, TRUE);
        }
        PF_AllocPage(tbl->fd, &(tbl->page_num), &(tbl->page_buf));
        setNumSlots(tbl->page_buf, 0);
        setFreePointer(tbl->page_buf, tbl->page_buf+PF_PAGE_SIZE);
        printf("tbl->page_buf+PF_PAGE_SIZE = %p", tbl->page_buf+PF_PAGE_SIZE);
    }

    int nslots = getNumSlots(tbl->page_buf);
    char* ptr = getFreePointer(tbl->page_buf);
    setFreePointer(tbl->page_buf, ptr-len);
    setNumSlots(tbl->page_buf, nslots+1);
    printf("%p, %d, %p\n", ptr, len, ptr-len);
    setNthSlotOffset(tbl->page_buf, nslots+1, ptr-len);
    memcpy(ptr-len, record, len);
    *rid = (tbl->page_num << 16) +(nslots+1);
    //printf("%d, %d, %d\n", tbl->page_num<<16, (nslots+1), *rid);
}

#define checkerr(err) {if (err < 0) {PF_PrintError(); exit(EXIT_FAILURE);}}

/*
  Given an rid, fill in the record (but at most maxlen bytes).
  Returns the number of bytes copied.
 */
int
Table_Get(Table *tbl, RecId rid, byte *record, int maxlen) {
    // PF_GetThisPage(pageNum)
    // In the page get the slot offset of the record, and
    // memcpy bytes into the record supplied.
    // Unfix the page
    char* buffer;
    int slot = rid & 0xFFFF;
    int pageNum = rid >> 16;
    
    if(pageNum == tbl->page_num){
        buffer = tbl->page_buf;
    }
    else {
        checkerr( PF_GetThisPage(tbl->fd, pageNum, &buffer) );
    }

    int len = getLen(slot, buffer);
    int offset = getNthSlotOffset(slot, buffer);
    memcpy(record, buffer+offset, maxlen<len ? maxlen : len);

    if(pageNum != tbl->page_num){
        PF_UnfixPage(tbl->fd, pageNum, TRUE);
    }

    return maxlen<len ? maxlen : len; // return size of record
}

void
Table_Scan(Table *tbl, void *callbackObj, ReadFunc callbackfn)
{
    // For each page obtained using PF_GetFirstPage and PF_GetNextPage
    //    for each record in that page,
    //          callbackfn(callbackObj, rid, record, recordLen)
    char* buffer;
    int page_num;

    int err = PF_GetFirstPage(tbl->fd, &page_num, &buffer);
    if(err == PFE_PAGEFIXED){
        buffer = tbl->page_buf;
        page_num = tbl->page_num;
    }
    while(err != PFE_EOF){
        for(int i=1; i<=getNumSlots(buffer); i++){
            int rid = page_num << 16 + i;
            char* record = buffer + getNthSlotOffset(i, buffer);
            printf("slot offset : %d\n", getNthSlotOffset(i, buffer));
            int recordLen = getLen(i, buffer);
            callbackfn(callbackObj, rid, record, recordLen);
        }
        
        err = PF_GetNextPage(tbl->fd, &page_num, &buffer);
        checkerr(err);
        if(err == PFE_PAGEFIXED){
            buffer = tbl->page_buf;
            page_num = tbl->page_num;
        }
    }
}


