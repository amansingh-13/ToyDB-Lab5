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
    //                      base          no_of_recs  ptr_to_free_space
    int* slot_list = (((byte*) pageBuf) + sizeof(int) + sizeof(char*));

    if(slot > 0)
        return slot_list[slot-1];
    else
        return PF_PAGE_SIZE;
}
int getLen(int slot, byte *pageBuf)
{
	int begin = getNthSlotOffset(slot,   (char*) pageBuf);
	int end   = getNthSlotOffset(slot-1, (char*) pageBuf);
    return end - begin;
}

// sets the pointer (offset) to the record in a slot 
void setNthSlotOffset(byte* pageBuf, int slot, char* ptr)
{
    int* slot_list = (((byte*) pageBuf) + sizeof(int) + sizeof(char*));
    slot_list[slot-1] = ptr - pageBuf;
}
// updates/sets the field which stores the pointer to free data
void setFreePointer(byte *pageBuf, char* ptr)
{
    *((char**)(pageBuf+sizeof(int))) = ptr;
}
// returns the address to the data that is free + 1
char* getFreePointer(byte* pageBuf)
{
    return *((char**)(pageBuf+sizeof(int)));
}
// returns the amount of free space left in the page
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

    // initialize the Table
    *ptable = (Table*) malloc(sizeof(Table));
    (*ptable)->fd = fd;
    (*ptable)->page_num = -1;
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
        // allocate new page if the free space is not enough to store new record
        if(tbl->page_buf != NULL){
            PF_UnfixPage(tbl->fd, tbl->page_num, TRUE);
        }
        PF_AllocPage(tbl->fd, &(tbl->page_num), &(tbl->page_buf));
        setNumSlots(tbl->page_buf, 0);
        setFreePointer(tbl->page_buf, tbl->page_buf+PF_PAGE_SIZE);
    }

    // set and update all the fields that are necessary
    int nslots = getNumSlots(tbl->page_buf);
    char* ptr = getFreePointer(tbl->page_buf);
    setFreePointer(tbl->page_buf, ptr-len);
    setNumSlots(tbl->page_buf, nslots+1);
    setNthSlotOffset(tbl->page_buf, nslots+1, ptr-len);
    memcpy(ptr-len, record, len);
    *rid = (tbl->page_num << 16) +(nslots+1);
}

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
    
    // if page is already open, no point in opening it again
    if(pageNum == tbl->page_num){
        buffer = tbl->page_buf;
    }
    else {
        checkerr( PF_GetThisPage(tbl->fd, pageNum, &buffer) );
    }

    // copy record to the memory pointed to by record
    int len = getLen(slot, buffer);
    int offset = getNthSlotOffset(slot, buffer);
    memcpy(record, buffer+offset, maxlen<len ? maxlen : len);

    // do not unfix if the open page has been used
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
    // keep looping till all pages are read
    while(err != PFE_EOF){
        for(int i=1; i<=getNumSlots(buffer); i++){
            int rid = page_num << 16 + i;
            char* record = buffer + getNthSlotOffset(i, buffer);
            int recordLen = getLen(i, buffer);
            callbackfn(callbackObj, rid, record, recordLen);
        }
        
        err = PF_GetNextPage(tbl->fd, &page_num, &buffer);

        // if already fixed, use the data saved in tbl struct
        if(err == PFE_PAGEFIXED){
            buffer = tbl->page_buf;
            page_num = tbl->page_num;
        }
    }
}
