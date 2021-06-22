A single-consumer single-producer bip buffer in C. 

The use case is for audio programming. In order to avoid data corruption due to read errors (too little or too much data) the scsp bip buffer will (attempt to (need to add some sort of granular control over read/write wait times) synchronize writing/reading between different threads. 
