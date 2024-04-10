#include<stdlib.h>
#include<stdio.h>
#include<omp.h>
#include<string.h>
#include<ctype.h>
#include<mtasker.h>

typedef char byte;

enum cache_state{
    INVALID,
    MODIFIED,
    SHARED,
    EXCLUSIVE
}


struct cache {
    byte address; // This is the address in memory.
    byte value; // This is the value stored in cached memory.
    // State for you to implement MESI protocol.
    enum cache_state;
};

struct decoded_inst {
    int type; // 0 is RD, 1 is WR
    byte address;
    byte value; // Only used for WR 
};
enum BusState{
    RDM,
    WRM,
    RWITM,
    INVALIDATE,
    DATARESP,
    NODATA
}
struct Bus{
    int sender;
    int receiver;
    enum BusState;
    byte address;
    byte value;
    enum cache_state;
};
typedef struct cache cache;
typedef struct decoded_inst decoded;
typedef struct Bus Bus;


/*
 * This is a very basic C cache simulator.
 * The input files for each "Core" must be named core_1.txt, core_2.txt, core_3.txt ... core_n.txt
 * Input files consist of the following instructions:
 * - RD <address>
 * - WR <address> <val>
 */

byte * memory;

// Decode instruction lines
decoded decode_inst_line(char * buffer){
    decoded inst;
    char inst_type[2];
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


// This function implements the mock CPU loop that reads and writes data.
void cpu_loop(int num_threads){
    // Initialize a CPU level cache that holds about 2 bytes of data.
    int memory_size = 24;
    memory = (byte *) malloc(sizeof(byte) * memory_size);
    omp_lock_t locks_arr[num_threads];
    Bus data_comm[num_threads] = {NULL};
    for (int i=0; i<num_threads; i++)
        omp_init_lock(&(locks_arr[i]));
    omp_set_nested(1);
    #pragma omp parallel num_threads(num_threads) shared(memory,data_comm,locks_arr)
    {
        int cache_size = 2;
        cache * c = (cache *) malloc(sizeof(cache) * cache_size);
        int thread_num = omp_get_thread_num();
        // Read Input file
        char filename[20];
        sprintf(filename, "input_%d.txt", thread_num);
        FILE * inst_file = fopen(filename, "r");
        #pragma omp parallel num_threads(2) shared(c,locks_arr,data_comm,memory)
        {
            #pragma omp sections{
                #pragma omp section
                {
                    char inst_line[20];
                // Decode instructions and execute them.
                    while (fgets(inst_line, sizeof(inst_line), inst_file))
                    {
                        decoded inst = decode_inst_line(inst_line);
                        /*
                        * Cache Replacement Algorithm
                        */
                        
                        int hash = inst.address%cache_size;
                        cache cacheline = *(c+hash);
                        /*
                        * This is where you will implement the coherancy check.
                        * For now, we will simply grab the latest data from memory.
                        */
                        if(cacheline.address != inst.address){
                            // Flush current cacheline to memory
                            *(memory + cacheline.address) = cacheline.value;
                            // Assign new cacheline
                            cacheline.address = inst.address;
                            cacheline.cache_state = EXCLUSIVE;
                            // This is where it reads value of the address from memory
                            cacheline.value = *(memory + inst.address);
                            if(inst.type == 1){
                                cacheline.value = inst.value;
                            }
                            *(c+hash) = cacheline;
                        }
                        switch(inst.type){
                            case 0:
                                //Read Hit
                            if(cacheline.address == inst.address)
                            {
                                //No change while reading from M,S,EX
                                if(cacheline.cache_state == MODIFIED || cacheline.cache_state == SHARED || cacheline.cache_state == EXCLUSIVE)
                                    printf("Reading from address %d: %d\n", cacheline.address, cacheline.value);

                                else{
                                    //INVALID CASE DONNO

                                }   
                                break;
                                
                            }
                            else{
                                // Read Miss
                                for(int i=0;i<num_threads;i++)
                                {
                                    if(i != thread_num)
                                    {
                                        omp_set_lock(&(locks_arr[i]));
                                        data_comm[i].sender = thread_num;
                                        data_comm[i].receiver = i;
                                        data_comm[i].address = inst.address;
                                        data_comm[i].value = inst.value;
                                        // data_comm[i].cache_state = cacheline.cache_state;
                                        data_comm[i].BusState = RDM;
                                        omp_unset_lock(&(locks_arr[i]));
                                    }
                                }

                            }
                            case 1:
                            {
                                //Write Hit
                                if(cacheline.address == inst.address)
                                {
                                    if(cacheline.cache_state == EXCLUSIVE){
                                        cacheline.cache_state = MODIFIED
                                        cacheline.value = inst.value;
                                    }
                                    
                                    else if(cacheline.cache_state == SHARED)
                                    {
                                        cacheline.cache_state = MODIFIED;
                                        cacheline.value = inst.value;
                                        for(int i=0;i<num_threads;i++)
                                        {
                                            if(i != thread_num)
                                            {
                                                omp_set_lock(&locks_arr[i]);
                                                //place INVALIDATE SIGNAL
                                                data_comm[i].sender = thread_num;
                                                data_comm[i].receiver = i;
                                                data_comm[i].address = cacheline.address;
                                                data_comm[i].value = cacheline.value;
                                                data_comm[i].cache_state = cacheline.cache_state;
                                                data_comm[i].BusState = INVALIDATE;
                                                omp_unset_lock(&locks_arr[i]);
                                            }
                                        }
                                    }
                                }
                                else
                                {
                                    //Write Miss
                                    for(int i=0;i<num_threads;i++)
                                    {
                                            if(i != thread_num)
                                            {
                                                omp_set_lock(&locks_arr[i]);
                                                //place INVALIDATE SIGNAL
                                                data_comm[i].sender = thread_num;
                                                data_comm[i].receiver = i;
                                                data_comm[i].address = cacheline.address;
                                                data_comm[i].value = cacheline.value;
                                                data_comm[i].cache_state = cacheline.cache_state;
                                                data_comm[i].BusState = WRM;
                                                omp_unset_lock(&locks_arr[i]);
                                            }
                                    }
                                    //Remember to give time gap

                                    //copy from memory
                                    cacheline.value = *(memory + inst.address);
                                    // write the data 
                                    cacheline.value = inst.value;
                                    //change the state to modified
                                    cacheline.cache_state = MODIFIED;
                                }
                                printf("Writing to address %d: %d\n", cacheline.address, cacheline.value);
                                break;
                            }
                        }
                    }
                    free(c);
                }
                #pragma omp section{
                    int count_nodata = 0;
                    while(1){
                        
                        if(data_comm[thread_num]!=NULL){

                        switch(data_comm[thread_num].BusState)
                        {
                            case RDM:{
                                int hash = data_comm[thread_num].address%cache_size;
                                cache cacheline = *(c+hash);
                                int to_resp = data_comm[thread_num].sender;

                                // If the data is in neighboring caches
                                if(cacheline.address == data_comm[thread_num].address){
                                
                                    if(cacheline.cache_state == MODIFIED){
                                        *(memory + cacheline.address) = cacheline.value;
                                    }
                                    cacheline.cache_state = SHARED;
                                    omp_set_lock(&(locks_arr[to_resp]));
                                    data_comm[to_resp].sender = thread_num;
                                    data_comm[to_resp].receiver = to_resp;
                                    data_comm[to_resp].address = cacheline.address;
                                    data_comm[to_resp].value = cacheline.value;
                                    data_comm[to_resp].BusState = DATARESP;
                                    data_comm[to_resp].cache_state = SHARED;
                                    omp_unset_lock(&(locks_arr[to_resp]));
                                }
                                else{
                                    omp_set_lock(&locks_arr[to_resp]);
                                    data_comm[to_resp].sender = thread_num;
                                    data_comm[to_resp].receiver = to_resp;
                                    data_comm[to_resp].BusState = NODATA;
                                    omp_set_lock(&locks_arr[to_resp]);
                                }
                                // put at the end
                                // omp_set_lock(&locks_arr[thread_num]);
                                // data_comm[thread_num] = NULL;
                                // omp_unset_lock(&locks_arr[thread_num]);
                                break;
                            }

                            case INVALIDATE: {
                                int hash = data_comm[thread_num].address%cache_size;
                                cache cacheline = *(c+hash);
                                    if(cacheline.address == data_comm[thread_num].address){
                                        cacheline.cache_state = INVALID;
                                    }
                                break;
                            }
                            
                            case WRM : {
                                int hash = data_comm[thread_num].address%cache_size;
                                cache cacheline = *(c+hash);
                                int to_resp = data_comm[thread_num].sender;

                                if(cacheline.address != data_comm[thread_num].address){
                                    omp_set_lock(&locks_arr[to_resp]);
                                    data_comm[to_resp].sender = thread_num;
                                    data_comm[to_resp].receiver = to_resp;
                                    data_comm[to_resp].BusState = NODATA;
                                    omp_set_lock(&locks_arr[to_resp]);
                                }
                                else{
                                    if(cacheline.cache_state==SHARED || cacheline.cache_state==EXCLUSIVE){
                                        cacheline.cache_state = INVALID
                                    }
                                    else if(cacheline.cache_state == MODIFIED){
                                        *(memory + cacheline.address) = cacheline.value;
                                        cacheline.cache_state = INVALID
                                    }
                                }
                                break;

                            }
                            case DATARESP : {
                                
                                break;
                            }
                            case NODATA : {

                                break;
                            }
                            }
                        }
                    }
                }
            }
        }
    }
}

int main(int c, char * argv[]){
    // Initialize Global memory
    // Let's assume the memory module holds about 24 bytes of data.
   
    int NUM_THREAD = 4;
    cpu_loop(NUM_THREAD);
    free(memory);
}