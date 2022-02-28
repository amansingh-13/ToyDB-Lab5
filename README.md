This assignment was done by

190020010 Aman Singh
190100036 Krushnakant Bhattad



Directory Structure










Explanation of the execution:

Task #1
In this task, we implemented the UNIMPLEMENTED parts 
based on the slotted page structure. 
The number of slots in the first four bytes is stored 
in the page, which we use in the getNumSlots and setNumSlots 
functions. 
The next 8 bytes store the free pointer, this fact 
is used in setFreePointer, getFreePointer functions.
After the first 12 bytes, 
we store the (offset) to the record in a slot,
sequentially. 



Task #2
In this task, we implemented the loadCSV API and 
requisite internal functions. The Encode function 
takes a schema, a row of data and encodes this 
into a single binary representation.
In this we make use of internal functions such as 
EncodeCString and their return values to achieve desired results


Task #3
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


