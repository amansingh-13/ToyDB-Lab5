This assignment was done by :

190020010 Aman Singh          [Task 1 + Task 3]
190100036 Krushnakant Bhattad [Task 2 + Task 3]



# Directory Structure :

```
190020010_190100036
|
`- README.md      #This is the current readme file with required info
`- screenshots/   #Screenshots as required
`- toydb/
  |
  `- amlayer/
  `- pflayer/
  `- dblayer/
  `- README.md    #This is the readme file from the problem statement
  `- am.pdf
  `- pf.pdf
```

# Instructions to Run
Run `make clean` followed by `make` in all 3 sub-folders of toydb
cd into dblayer/ , and run ./loaddb 
followed by    ./dumpdb s     OR    ./dumpdb i
as required.


# Explanation of the execution:

## Task #1
In this task, we implemented the UNIMPLEMENTED parts 
based on the slotted page structure. 
The number of slots in the first four bytes is stored 
in the page, which we use in the getNumSlots and setNumSlots 
functions. 
The next 8 bytes store the free pointer, this fact 
is used in setFreePointer, getFreePointer functions.
After the first 12 bytes, 
we store the pointers (offsets) to the records in a slot,
sequentially. The functions getSlotOffset and setSlotOffset
handle this functionality.
The table struct holds the database's file descriptor, the 
schema and the page number and buffer pointer of the 
page currently fixed in memory.
All the other functions are implemented as described in
the comments.



## Task #2
In this task, we implemented the loadCSV API and 
requisite internal functions. The Encode function 
takes a schema, a row of data and encodes this 
into a single binary representation.
In this we make use of internal functions such as 
EncodeCString and their return values to achieve desired results


## Task #3
We implement the index_scan, Table_Scan and 
requisite internal functions. 
The index_scan function uses the AM Layer API 
to get the recIDs of relevant rows from the index file 
and then we use the Table API to extract further 
information based on the recID.
Similarly for Table_Scan we do a sequential scan 
using the PF layer API.
In both cases, the printRow function is used to decode
the data extracted 
(This function, in spirit, does 
opposite of what the encode function does) 

