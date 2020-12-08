/*  
*   CS:APP Cache Simulator
*   Cache Simulator with a least recently used eviction policy.
*   Reads a valgrind trace file and simulates misses and hits
*   for a cache with S sets, E lines, and b offset bits.
*   Operates by pulling the important information from a valgrind trace   
*   and storing it in a struct, where one struct holds the info
*   from one line of the trace file. The cache simulator works
*   by reading this struct.
*/

#include "cachelab.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

// Simulated cache block, lri is the least recently used index
// The block with the lowest index will be evicted 
typedef struct {
    bool valid_bit;
    int tag;
    int lri;
} block;

// Simulated cache set with a set index and array of E blocks
typedef struct {
    unsigned int index;
    block *arr_blocks;
} set;

typedef struct {
    enum {LOAD, STORE, MODIFY} type;
    unsigned long address;
    bool end;
} trace_line;

// Global variables for the cache
int sets;
int lines;
int block_bits;
int lri_count;
int miss;
int hit;
int evictions;
unsigned int tag_index[2];  // Holds the tag and index from an address respectively

set *cache;

// String for tracefile name needed for error message printing
char *trace_name;

// Function prototypes
static void cache_init(int s, int E, int b);
static void print_help();
static void print_trace_lines(trace_line *trace);
static void parse_trace(trace_line *trace, int s, int b, bool is_verbose);
static trace_line *read_trace(const char *tracefile);
static void parse_address(unsigned long address, int s, int b);

int main(int argc, char **argv) {
    int s = -1;
    int E = -1;
    int b = -1;

    // Array of trace_lines read from file 
    trace_line *trace;

    bool is_verbose = false;

    lri_count = 0;
    hit = miss = evictions = 0;

    char *trace_file = NULL;
    char opt;
    
    while((opt = getopt(argc, argv, "vhs:E:t:b:")) != -1) {
        switch (opt) {
            case 's':
                s = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break; 
            case 'b':
                b = atoi(optarg);
                break;
            case 't':
                trace_file = optarg;
                trace_name = optarg;
                break;
            case 'h':
                print_help();
                break;
            case 'v':
                is_verbose = true;
                break;
            case ':':
                printf("test");
                break;
            default:
                break;
        }
    }

    if (argc == 1) {
        fprintf(stderr, "./csim: Missing required command line argument\n");
        print_help();
        return 1;
    }



    printf("%d, %d, %d, %s \n", s, E, b, trace_file);

    cache_init(s, E, b);
    trace = read_trace(trace_file);
    parse_trace(trace, s, b, is_verbose);
    free(cache);
    printSummary(hit, miss, evictions);
    return 0;
}

// Creates simulated cache with s set index bits,
// E cache lines, and b block bits
static void cache_init(int s, int E, int b) {
    sets = 1 << s;
    lines = E;
    block_bits = 1 << b;

    // Allocate memory for the cache
    // cache is an array of sets
    cache = (set *) malloc(sets * sizeof(set));

    // Iterate through sets and allocate memory for the blocks
    // Initalize sets ands blocks
    for (int i = 0; i < sets; i++) {
        cache[i].index = i;
        cache[i].arr_blocks = (block *) malloc(lines * sizeof(block));

        for (int j = 0; j < lines; j++) {
            cache[i].arr_blocks[j].valid_bit = false;
            cache[i].arr_blocks[j].tag = -1;
            cache[i].arr_blocks[j].lri = -1;
        }
    }
}

static void print_help() {
    printf("Usage: ./csim [-hv] -s <num> -E <num> -b <num> -t <file>\n");
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n");
    printf("\n");
    printf("Examples: \n");
    printf("  linux>  ./csim -s 4 -E 1 -b 4 -t traces/yi.trace\n");
    printf("  linux>  ./csim -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}

// Extract the relevant information from a valgrind trace file
// and store in an array of trace_lines
static trace_line *read_trace(const char *tracefile) {
    FILE *fp;
    int line_len = 15; // max length of chars in a valgrind trace
    char str_line[line_len + 2]; // space for newline and null terminator
    unsigned long address;

    if ((fp = fopen(tracefile, "r")) == NULL) {
        if (trace_name !=  NULL)
            fprintf(stderr, "%s: No such file or directory\n", trace_name);

        print_help();     
        exit(0);
    }

    // Moves file position to the EOF, returns 0 if successful    
    if (fseek(fp, 0, SEEK_END) == 0) {
        long size;

        // Returns size of file in bytes (char), returns -1 if unsuccessful
        if ((size = ftell(fp)) == -1) {
            fprintf(stderr, "File error: %s\n", strerror(errno));
            exit(0);
        }

        // Moves file position to the start of the file
        if (fseek(fp, 0, SEEK_SET) != 0) {
            fprintf(stderr, "File error: %s\n", strerror(errno));
            exit(0);
        }

        // Create space for the array of trace_lines
        trace_line *trace = (trace_line *) malloc(size * sizeof(trace_line));

        int i = 0;  // Holds index for trace_line array

        while (fgets(str_line, line_len + 2, fp)) {
            char type = str_line[1];
            char *token;
            char *ptr;  // endptr for strtol
            // Isolate address
            strtok(str_line, " ");
            token = strtok(NULL, ",");  

            // Process type
            if (type == 'L')
                trace[i].type = LOAD;
            else if (type == 'S')
                trace[i].type = STORE;
            else if (type == 'M')
                trace[i].type = MODIFY;
            else {  // Skip I
                continue;
            }

            // Process address
            address = strtoul(token, &ptr, 16);  
            trace[i].address = address;

            trace[i].end = false;
            i++;
        }
        fclose(fp);

        // Create end trace
        
        trace[i].end = true;
        
        return trace;
       } else {
           fprintf(stderr, "File error: %s\n", strerror(errno));
           exit(0);
       }
}

// For debugging
static void print_trace_lines(trace_line *trace) {
    int i = 0;

    while (!trace->end) {
        printf("Info for trace number %d\n", i);
        printf("Type: %d\n", trace->type);
        printf("Address: %ld\n", trace->address);
        printf("---------------------------\n");
        i++;
        trace++;
    }
}


// Main cache sim function, read the trace_line info and execute instructions
static void parse_trace(trace_line *trace, int s, int b, bool is_verbose) {
    block *current_line;
    block *first_line;
    set *current_set;
    bool is_hit;  
    bool found_empty;
    bool is_type_m = false;

    while (!trace->end) {
        if (trace->type == 2) 
            is_type_m = true;

        parse_address(trace->address, s, b);

        start:  // Repeat once for modify instruction

        is_hit = false;
        found_empty = false;

        current_set = cache;

        // Locate desired set, assumes the set index is valid
        first_line = current_line = current_set[tag_index[1]].arr_blocks;

        // Find the matching tag by iterating through lines
        for (int i = 0; i < lines; i++) {
            if (current_line->tag == tag_index[0] && current_line->valid_bit) { // Hit
                hit++;
                is_hit = true;
                current_line->lri = lri_count;
                lri_count++;

                if (is_verbose) 
                    printf("%d Hit %lx\n", trace->type, trace->address);
                
                break;
            }
            current_line++;
        }

        if (!is_hit) { 
            miss++;
            current_line = first_line;

            // Find empty line, if found, update tag, valid bit, and lri
            for (int i = 0; i < lines; i++) {
                if (!current_line->valid_bit) { 
                    current_line->tag = tag_index[0];
                    current_line->valid_bit = true;
                    current_line->lri = lri_count;
                    lri_count++;
                    found_empty = true;

                    if (is_verbose) 
                        printf("%d Miss %lx\n", trace->type, trace->address);

                    // Load or store data...
                    break;
                }
                current_line++;
            }

            if (!found_empty) { // Evict
                evictions++;
                block *min_lri = first_line; // Points to least recently used line

                if (lines > 1) {   
                    current_line = first_line + 1;

                    for (int i = 1; i < lines; i++) {
                        if (current_line->lri < min_lri->lri) 
                            min_lri = current_line;
                        
                        current_line++;
                    }
                }
                // Overwrite existing info and update lri_count
                min_lri->tag = tag_index[0];
                min_lri->lri = lri_count;
                lri_count++;

                if (is_verbose) 
                    printf("%d Eviction %lx\n", trace->type, trace->address);
            }
        }

        if (is_type_m) {
            is_type_m = false;
            goto start;
        }
        trace++;
    }
}

// Takes a valgrind address and returns the set index and tag
// in a length 2 array [set_index, tag]
static void parse_address(unsigned long address, int s, int b) {
    unsigned int set_index; // Ensure logical right shifts with unsigned
    unsigned int tag;   

    set_index = (address >> b) & ~(~0ul << s);  // Cast mask to unsigned long for correct # of bits
    tag = address >> (s + b);
    tag_index[0] = tag;
    tag_index[1]= set_index;
}