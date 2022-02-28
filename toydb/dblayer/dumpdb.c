#include <stdio.h>
#include <stdlib.h>
#include "codec.h"
#include "tbl.h"
#include "util.h"
#include "../pflayer/pf.h"
#include "../amlayer/am.h"
#include <string.h>
#define checkerr(err) {if (err < 0) {PF_PrintError(); exit(1);}}
#define UNIMPLEMENTED (1==1)

void
printRow(void *callbackObj, RecId rid, byte *row, int len) {
    Schema *schema = (Schema *) callbackObj;
    byte *cursor = row;

    int length,x,y;
    long z;

    for(int i=0; i<schema->numColumns; i++){
        switch(schema->columns[i]->type){

            case VARCHAR:
                length = DecodeShort(cursor);
                char* str = malloc(length+1);
                x = DecodeCString(cursor, str, length+2);
                printf("%s,", str);
                cursor+=(x+2);
                free(str);
                break;

            case INT:
                y = DecodeInt(cursor);
                printf("%d\n", y);
                cursor+=4;
                break;

            case LONG:
                z = DecodeLong(cursor);
                printf("%d\n", y);
                cursor+=4;
                break;

            default:
                fprintf(stderr, "Error in Schema ColumnDesc: Type must be one of VARCHAR, INT or LONG");
                exit(EXIT_FAILURE);
        }
    }
    UNIMPLEMENTED;
}

#define DB_NAME "data.db"
#define INDEX_NAME "data.db.0"
	 
void
index_scan(Table *tbl, Schema *schema, int indexFD, int op, int value) {
    // Open index ...
    // while (true) {
	// find next entry in index
	// fetch rid from table
    //     printRow(...)
    // }
    // close index ...
    int recId;
    int scanFD = AM_OpenIndexScan(indexFD, 'i', 4, op, (char*) &value);

    byte* buff = (byte*) malloc(PF_PAGE_SIZE);
    while((recId = AM_FindNextEntry(scanFD)) != AME_EOF){
        memset(buff, 0, PF_PAGE_SIZE);
        int recLen = Table_Get(tbl, recId, buff, PF_PAGE_SIZE);
        printRow(schema, recId, buff, recLen);
    }
    free(buff);

    AM_CloseIndexScan(scanFD);
}

int
main(int argc, char **argv) {
    char *schemaTxt = "Country:varchar,Capital:varchar,Population:int";
    Schema *schema = parseSchema(schemaTxt);
    Table *tbl;

    int err = Table_Open(DB_NAME, schema, false, &tbl);
    checkerr(err);

    UNIMPLEMENTED;

    if (argc == 2 && *(argv[1]) == 's') {
        UNIMPLEMENTED;
        // invoke Table_Scan with printRow, which will be invoked for each row in the table.
        Table_Scan(tbl, schema, printRow);
    } else {
        // index scan by default
        int indexFD = PF_OpenFile(INDEX_NAME);
        checkerr(indexFD);

        // Ask for populations less than 100000, then more than 100000. Together they should
        // yield the complete database.
        index_scan(tbl, schema, indexFD, LESS_THAN_EQUAL, 100000);
        index_scan(tbl, schema, indexFD, GREATER_THAN, 100000);
    }
    Table_Close(tbl);
}
