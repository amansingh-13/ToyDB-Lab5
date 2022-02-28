#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include "codec.h"
#include "../pflayer/pf.h"
#include "../amlayer/am.h"
#include "tbl.h"
#include "util.h"

#define checkerr(err) {if (err < 0) {PF_PrintError(); exit(1);}}

#define MAX_PAGE_SIZE 4000


#define DB_NAME "data.db"
#define INDEX_NAME "data.db.0"
#define CSV_NAME "data.csv"


/*
Takes a schema, and an array of strings (fields), and uses the functionality
in codec.c to convert strings into compact binary representations
 */
int
encode(Schema *sch, char **fields, byte *record, int spaceLeft) {
    //UNIMPLEMENTED;

    int init_space = spaceLeft;

    for(int i=0; i<sch->numColumns; i++){
        switch(sch->columns[i]->type){
            case VARCHAR:
                int x = EncodeCString(fields[i], record, spaceLeft);
                spaceLeft-=x;
                record+=x;
                break;
            case INT:
                int x = EncodeInt(fields[i], record);
                spaceLeft-=x;
                record+=x;
                break;
            case LONG:
                int x = EncodeLong(fields[i], record);
                spaceLeft-=x;
                record+=x;
                break;
            default:
                fprintf(stderr, "Error in Schema ColumnDesc: Type must be one of VARCHAR, INT or LONG");
                exit(EXIT_FAILURE);
        }
    }

    return init_space-spaceLeft;

    // for each field
    //    switch corresponding schema type is
    //        VARCHAR : EncodeCString
    //        INT : EncodeInt
    //        LONG: EncodeLong
    // return the total number of bytes encoded into record
}

Schema *
loadCSV() {

    int err;

    // Open csv file, parse schema
    FILE *fp = fopen(CSV_NAME, "r");
    if (!fp) {
	perror("data.csv could not be opened");
        exit(EXIT_FAILURE);
    }

    char buf[MAX_LINE_LEN];
    char *line = fgets(buf, MAX_LINE_LEN, fp);
    if (line == NULL) {
        fprintf(stderr, "Unable to read data.csv\n");
        exit(EXIT_FAILURE);
    }

    
    // Open main db file
    Schema *sch = parseSchema(line);
    Table *tbl;

    int r = Table_Open(DB_NAME, sch, true, &tbl);
    err = AM_CreateIndex(DB_NAME, 0, 'i', 4);
    checkerr(err);

    int indexFD = PF_OpenFile(INDEX_NAME);

    UNIMPLEMENTED;

    char *tokens[MAX_TOKENS];
    char record[MAX_PAGE_SIZE];

    while ((line = fgets(buf, MAX_LINE_LEN, fp)) != NULL) {
        int n = split(line, ",", tokens);
        assert (n == sch->numColumns);
        int len = encode(sch, tokens, record, sizeof(record));
        RecId rid;

        Table_Insert(tbl, record, len, &rid);

        UNIMPLEMENTED;

        printf("%d %s\n", rid, tokens[0]);

        // Indexing on the population column 
        int population = atoi(tokens[2]);

        char attrType = 'i';
        int attrLength = 4;


        err = AM_InsertEntry(
            indexFD, attrType, attrLength, 
            (char*)&population, 
            rid
        );

        UNIMPLEMENTED;
        // Use the population field as the field to index on
        checkerr(err);
    }

    fclose(fp);
    Table_Close(tbl);
    err = PF_CloseFile(indexFD);
    checkerr(err);
    return sch;
}

int
main() {
    loadCSV();
}
