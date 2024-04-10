#include<stdlib.h>
#include<stdio.h>
#include<omp.h>
#include<string.h>
#include<ctype.h>

typedef char byte;

struct cache {
    byte address; // This is the address in memory.
    byte value; // This is the value stored in cached memory.
    // State for you to implement MESI protocol.
    byte state; // M: Modified, E: Exclusive, S: Shared, I: Invalid
};

struct decoded_inst {
    int type; // 0 is RD, 1 is WR
    byte address;
    byte value; // Only used for WR 
};

typedef struct cache cache;
typedef struct decoded_inst decoded;

byte * memory;

// Decode instruction lines
decoded decode_inst_line(char * buffer){
    decoded inst;
    char inst_type[3]; // Increased size to accommodate "WR"
    sscanf(buffer, "%s", inst_type);
    if(!strcmp(inst_type, "RD")){
        inst.type = 0;
        int addr = 0;
        sscanf(buffer, "%s %d", inst_type, &addr);
        inst.value = -1;
        inst.address = addr;
    } else if(!strcmp(inst_type, "WR")){
        inst.type = 1;
        int addr = 0;
        int val = 0;
        sscanf(buffer, "%s %d %d", inst_type, &addr, &val);
        inst.address = addr;
        inst.value = val;
    }
    return inst;
}

// Helper function to print the cachelines
void print_cachelines(cache * c, int cache_size){
    for(int i = 0; i < cache_size; i++){
        cache cacheline = *(c+i);
        printf("Address: %d, State: %c, Value: %d\n", cacheline.address, cacheline.state, cacheline.value);
    }
}

// MESI Protocol functions
void read_from_memory(cache * cacheline, byte * memory, byte address) {
    cacheline->value = memory[address];
    cacheline->state = 'S'; // Set to Shared
}

void write_to_memory(cache * cacheline, byte * memory, byte address, byte value) {
    memory[address] = value;
    cacheline->value = value;
    cacheline->state = 'M'; // Set to Modified
}
// 
// This function implements the mock CPU loop that reads and writes data.
void cpu_loop(int num_threads){
    FILE * inst_file[num_threads];
    char inst_line[num_threads][20];

    // Shared memory
    int memory_size = 24;
    memory = (byte *) malloc(sizeof(byte) * memory_size);
    for (int i = 0; i < memory_size; i++) {
        memory[i] = 0; // Initialize memory contents to 0
    }
    
    #pragma omp parallel num_threads(num_threads) shared(inst_file, inst_line, memory)
    {
        int tid = omp_get_thread_num();
        int cache_size = 2;
        cache * c = (cache *) malloc(sizeof(cache) * cache_size);
        
        // Initialize cache for this thread
        for (int i = 0; i < cache_size; i++) {
            (c + i)->state = 'I'; // Initialize cache lines to Invalid state
        }

        // Open input file for this thread
        char filename[20];
        sprintf(filename, "input_%d.txt", tid);
        inst_file[tid] = fopen(filename, "r");

        // Decode instructions and execute them.
        while (fgets(inst_line[tid], sizeof(inst_line[tid]), inst_file[tid])) {
            decoded inst = decode_inst_line(inst_line[tid]);
            int hash = inst.address % cache_size;
            cache * cacheline = c + hash;
            // Cache coherence protocol check
            #pragma omp critical
            {
                switch (inst.type) {
                    case 0: // RD
                        if (cacheline->address != inst.address || cacheline->state == 'I') {
                            read_from_memory(cacheline, memory, inst.address);
                        }
                        break;

                    case 1: // WR
                        if (cacheline->address != inst.address || cacheline->state != 'M') {
                            write_to_memory(cacheline, memory, inst.address, inst.value);
                        }
                        break;
                }
            }
            // Print access information
            printf("Thread %d: %s %d: %d\n", tid, inst.type == 0 ? "RD" : "WR", inst.address, cacheline->value);
        }

        // Close file and free memory for this thread
        fclose(inst_file[tid]);
        free(c);
    }

    // Free shared memory
    free(memory);
}

int main(int argc, char * argv[]){
    if (argc != 2) {
        printf("Usage: %s <num_threads>\n", argv[0]);
        return 1;
    }
    int num_threads = atoi(argv[1]);
    if (num_threads <= 0) {
        printf("Invalid number of threads.\n");
        return 1;
    }
    cpu_loop(num_threads);
    return 0;
}

// ----------------------------------------------------------------------------------------------------------------------------------- 


/* 
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<omp.h>

typedef char byte;

// Cache states for MESI protocol
#define INVALID 0
#define MODIFIED 1
#define EXCLUSIVE 2
#define SHARED 3

struct cache {
    byte address; // This is the address in memory.
    byte value; // This is the value stored in cached memory.
    byte state; // Cache state for MESI protocol.
    omp_lock_t lock; // Lock for synchronizing cache updates
};

struct decoded_inst {
    int type; // 0 is RD, 1 is WR
    byte address;
    byte value; // Only used for WR 
};

typedef struct cache cache;
typedef struct decoded_inst decoded;

byte * memory;

// Decode instruction lines
decoded decode_inst_line(char * buffer){
    decoded inst;
    char inst_type[3];
    sscanf(buffer, "%s", inst_type);
    if(!strcmp(inst_type, "RD")){
        inst.type = 0;
        int addr = 0;
        sscanf(buffer, "%s %d", inst_type, &addr);
        inst.value = -1;
        inst.address = addr;
    } else if(!strcmp(inst_type, "WR")){
        inst.type = 1;
        int addr = 0;
        int val = 0;
        sscanf(buffer, "%s %d %d", inst_type, &addr, &val);
        inst.address = addr;
        inst.value = val;
    }
    return inst;
}

// Helper function to print the cachelines
void print_cachelines(cache * c, int cache_size){
    for(int i = 0; i < cache_size; i++){
        cache cacheline = *(c+i);
        printf("Address: %d, State: %d, Value: %d\n", cacheline.address, cacheline.state, cacheline.value);
    }
}

// MESI protocol functions
void handle_read(cache * cacheline) {
    switch(cacheline->state) {
        case INVALID:
            // Read miss
            // Fetch data from memory
            cacheline->value = memory[cacheline->address];
            cacheline->state = SHARED;
            break;
        case MODIFIED:
        case EXCLUSIVE:
        case SHARED:
            // Cache hit, no action required
            break;
    }
}

void handle_write(cache * cacheline, byte value) {
    switch(cacheline->state) {
        case INVALID:
            // Write miss
            // Fetch data from memory
            cacheline->value = memory[cacheline->address];
            cacheline->state = MODIFIED;
            break;
        case MODIFIED:
            // Cache hit, update value
            cacheline->value = value;
            break;
        case EXCLUSIVE:
        case SHARED:
            // Cache hit, update value and change state to MODIFIED
            cacheline->value = value;
            cacheline->state = MODIFIED;
            break;
    }
}

// This function implements the mock CPU loop that reads and writes data.
void cpu_loop(int num_threads){
    // Initialize a CPU level cache that holds about 2 bytes of data.
    int cache_size = 2;
    cache * c = (cache *) malloc(sizeof(cache) * cache_size);
    
    // Initialize locks
    for (int i = 0; i < cache_size; i++) {
        omp_init_lock(&(c[i].lock));
    }

    // Read Input file
    FILE * inst_file = fopen("input_0.txt", "r");
    char inst_line[20];
    // Decode instructions and execute them.
    while (fgets(inst_line, sizeof(inst_line), inst_file)){
        decoded inst = decode_inst_line(inst_line);
        // /* 
        //  * Cache Replacement Algorithm
        //  

        int hash = inst.address % cache_size;
        cache * cacheline = &(c[hash]);
        
        omp_set_lock(&(cacheline->lock));
        
        if(cacheline->address != inst.address){
            // Flush current cacheline to memory
            memory[cacheline->address] = cacheline->value;
            // Assign new cacheline
            cacheline->address = inst.address;
            cacheline->state = INVALID;
            // This is where it reads value of the address from memory
            cacheline->value = memory[inst.address];
            if(inst.type == 1){
                cacheline->value = inst.value;
            }
        } else {
            // Cache hit
            if(inst.type == 0) {
                handle_read(cacheline);
            } else {
                handle_write(cacheline, inst.value);
            }
        }

        // Output
        switch(inst.type){
            case 0:
                printf("Thread %d: RD %d: %d\n", omp_get_thread_num(), cacheline->address, cacheline->value);
                break;
            
            case 1:
                printf("Thread %d: WR %d: %d\n", omp_get_thread_num(), cacheline->address, cacheline->value);
                break;
        }
        
        omp_unset_lock(&(cacheline->lock));
    }
    
    // Clean up
    fclose(inst_file);
    free(c);
}

int main(int argc, char * argv[]){
    // Initialize Global memory
    // Let's assume the memory module holds about 24 bytes of data.
    int memory_size = 24;
    memory = (byte *) malloc(sizeof(byte) * memory_size);
    
    // Set memory values
    for (int i = 0; i < memory_size; i++) {
        memory[i] = 0;
    }
    
    // Initialize OpenMP
    int num_threads = 4; // Change this to the number of threads you want to use
    omp_set_num_threads(num_threads);
    
    #pragma omp parallel
    {
        cpu_loop(num_threads);
    }

    // Clean up
    free(memory);

    return 0;
}
 */