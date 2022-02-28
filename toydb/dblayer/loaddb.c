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

/*  Takes a schema, and an array of strings (fields), and uses the functionality
in codec.c to convert strings into compact binary representations */
int
encode(Schema *sch, char **fields, byte *record, int spaceLeft) {

    int init_space = spaceLeft;
    int x;

    /*
        Iterate over fields in the schema, 
        encode on a case to case basis 
        based on datatype
    */
    // for each field
    //    switch corresponding schema type is
    //        VARCHAR : EncodeCString
    //        INT : EncodeInt
    //        LONG: EncodeLong
    for(int i=0; i<sch->numColumns; i++){
        switch(sch->columns[i]->type){

            //Encoding function Returns the total number of bytes encoded (including the length)
            //Mpve further the record by the encoded length to begin next encoding

            case VARCHAR:
                x = EncodeCString(fields[i], record, spaceLeft);
                spaceLeft-=x;
                record+=x;
                break;
            case INT:
                x = EncodeInt(atoi(fields[i]), record);
                spaceLeft-=x;
                record+=x;
                break;
            case LONG:
                x = EncodeLong(atol(fields[i]), record);
                spaceLeft-=x;
                record+=x;
                break;

            //If the column type is not varchar, int or long, there is some error

            default:
                fprintf(stderr, "Error in Schema ColumnDesc: Type must be one of VARCHAR, INT or LONG");
                exit(EXIT_FAILURE);
        }
    }

    // return the total number of bytes encoded into record
    return init_space-spaceLeft;
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

    //Init functions
    int r = Table_Open(DB_NAME, sch, true, &tbl);
    err = AM_CreateIndex(DB_NAME, 0, 'i', 4);
    checkerr(err);

    //Open the secondary file created by AM_CreateIndex above
    int indexFD = PF_OpenFile(INDEX_NAME);

    char *tokens[MAX_TOKENS];
    char record[MAX_PAGE_SIZE];

    // Get each line from the csv file into the buffer, 
    // and work on it
    
    while ((line = fgets(buf, MAX_LINE_LEN, fp)) != NULL) {
        int n = split(line, ",", tokens);
        assert (n == sch->numColumns);

        // Encode the data into contiguous memory in "record"
        int len = encode(sch, tokens, record, sizeof(record));
        RecId rid;

        // Insert the encoded data into the table
        Table_Insert(tbl, record, len, &rid);

        // Indexing on the population column 
        int population = atoi(tokens[2]);

        char attrType = 'i';
        int attrLength = 4;

        // Insert the entry into the B-Tree
        err = AM_InsertEntry(
            indexFD, attrType, attrLength, 
            (char*)&population, 
            rid
        );

        checkerr(err);
    }

    // Close all the files
    fclose(fp);
    Table_Close(tbl);
    err = PF_CloseFile(indexFD);
    checkerr(err);

    // Return schema
    return sch;
}

int
main() {
    loadCSV();
}
