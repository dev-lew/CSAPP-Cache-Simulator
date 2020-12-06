# CSAPP-Cache-Simulator

Cache Simulator with a least recently used eviction policy.
Reads a valgrind trace file and simulates misses and hits
for a cache with S sets, E lines, and b offset bits.
Operates by pulling the important information from a valgrind trace   
and storing it in a struct, where one struct holds the info
from one line of the trace file. The cache simulator works
by reading this struct.

Makefile requires x86-64 system and the other CS:APP files 

## Usage
linux> ./csim [-hv] -s <num> -E <num> -b <num> -t <tracefile>\n
