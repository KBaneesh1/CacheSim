#include<stdlib.h>
#include<stdio.h>
#include<omp.h>
#include<string.h>
#include<ctype.h>
// #include<mtasker.h>
// #include<time>
typedef char byte;

enum cache_state{
    INVALID,
    MODIFIED,
    SHARED,
    EXCLUSIVE
};

struct cache {
    byte address; // This is the address in memory.
    byte value; // This is the value stored in cached memory.
    // State for you to implement MESI protocol.
    enum cache_state cache_state;
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
    NODATARDM,
    NODATAWRM
};
struct Bus{
    int sender;
    int receiver;
    enum BusState BusState;
    byte address;
    byte value;
    enum cache_state cache_state;
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
    char inst_type[3];
    sscanf(buffer, "%s", inst_type);
    if(strcmp(inst_type, "RD")==0){
        inst.type = 0;
        int addr = 0;
        sscanf(buffer, "%s %d", inst_type, &addr);
        inst.value = -1;
        inst.address = addr;
    } else if(strcmp(inst_type, "WR")==0){
        int addr = 0;
        int val = 0;
        sscanf(buffer, "%s %d %d", inst_type, &addr, &val);
        inst.type = 1;
        inst.address = addr;
        inst.value = val;
    }
    // printf("instruction %s %d %d %d\n",inst_type,inst.type,inst.address,inst.value);
    return inst;
}

// Helper function to print the cachelines
void print_cachelines(cache * c, int cache_size){
    for(int i = 0; i < cache_size; i++){
        cache cacheline = *(c+i);
        printf("Address: %d, State: %d, Value: %d\n", cacheline.address, cacheline.cache_state, cacheline.value);
    }
}


// This function implements the mock CPU loop that reads and writes data.
void cpu_loop(int num_threads){
    // Initialize a CPU level cache that holds about 2 bytes of data.
    int memory_size = 24;
    memory = (byte *) malloc(sizeof(byte) * memory_size);
    omp_lock_t locks_arr[num_threads];
    struct Bus* data_comm[num_threads];
    // Initialize all pointers to NULL
    for (int i = 0; i < num_threads; ++i) {
        data_comm[i] = NULL;
    }
    for (int i=0; i<num_threads; i++)
        omp_init_lock(&(locks_arr[i]));
    omp_set_nested(1);
    #pragma omp parallel num_threads(num_threads) shared(memory,data_comm,locks_arr)
    {
        int instruction_finish = 0;
        int cache_size = 2;
        cache * c = (cache *) malloc(sizeof(cache) * cache_size);
        int thread_num = omp_get_thread_num();
        // Read Input file
        
        char filename[20];
        sprintf(filename, "input_%d.txt", thread_num);
        FILE * inst_file = fopen(filename, "r");
        #pragma omp parallel num_threads(2) shared(c,locks_arr,data_comm,memory)
        {
            #pragma omp sections
            {
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
                        // printf("%d %d %d\n",inst.type,inst.address,inst.value);
                        int hash = inst.address%cache_size;
                        cache cacheline = *(c+hash);
                        /*
                        * This is where you will implement the coherancy check.
                        * For now, we will simply grab the latest data from memory.
                        */
                        // if(cacheline.address != inst.address){
                        //     // Flush current cacheline to memory
                        //     *(memory + cacheline.address) = cacheline.value;
                        //     // Assign new cacheline
                        //     cacheline.address = inst.address;
                        //     cacheline.cache_state = EXCLUSIVE;
                        //     // This is where it reads value of the address from memory
                        //     cacheline.value = *(memory + inst.address);
                        //     if(inst.type == 1){
                        //         cacheline.value = inst.value;
                        //     }
                        // }
                        // printf("%d\n",inst.type);
                        // printf("Thread %d: %s %d: %d\n", thread_num, inst.type == 0 ? "RD" : "WR", inst.    , cacheline.value);
                        // print_cachelines(c,2);
                        switch(inst.type){
                            case 0:
                                //Read Hit
                            printf("inside read hit\n");
                            if(cacheline.address == inst.address)
                            {
                                //No change while reading from M,S,EX
                                if(cacheline.cache_state == MODIFIED || cacheline.cache_state == SHARED || cacheline.cache_state == EXCLUSIVE)
                                    // printf("Reading from address %d: %d\n", cacheline.address, cacheline.value);
                                    break;
                                else{
                                    //INVALID READ HIT
                                    for(int i=0;i<num_threads;i++)
                                    {
                                        if(i != thread_num)
                                        {
                                            omp_set_lock(&(locks_arr[i]));
                                            data_comm[i]->sender = thread_num;
                                            data_comm[i]->receiver = i;
                                            data_comm[i]->address = inst.address;
                                            data_comm[i]->value = inst.value;
                                            // data_comm[i]->cache_state = cacheline.cache_state;
                                            data_comm[i]->BusState = RDM;
                                            omp_unset_lock(&(locks_arr[i]));
                                        }
                                    }
                                }       
                            }

                            else{
                                // Read Miss
                                printf("inside read miss\n");

                                for(int i=0;i<num_threads;i++)
                                {
                                    if(i != thread_num)
                                    {
                                        omp_set_lock(&(locks_arr[i]));
                                        data_comm[i]->sender = thread_num;
                                        data_comm[i]->receiver = i;
                                        data_comm[i]->address = inst.address;
                                        data_comm[i]->value = inst.value;
                                        // data_comm[i]->cache_state = cacheline.cache_state;
                                        data_comm[i]->BusState = RDM;
                                        omp_unset_lock(&(locks_arr[i]));
                                    }
                                }

                                break;
                            }
                            case 1:
                            {
                                //Write Hit
                            printf("inside write hit\n");

                                if(cacheline.address == inst.address)
                                {
                                    if(cacheline.cache_state == EXCLUSIVE)
                                    {
                                        cacheline.cache_state = MODIFIED;
                                        cacheline.value = inst.value;
                                        printf("changing state from Exclusive to shared in thread %d\n",thread_num);
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
                                                data_comm[i]->sender = thread_num;
                                                data_comm[i]->receiver = i;
                                                data_comm[i]->address = cacheline.address;
                                                data_comm[i]->value = cacheline.value;
                                                data_comm[i]->cache_state = cacheline.cache_state;
                                                data_comm[i]->BusState = INVALIDATE;
                                                omp_unset_lock(&locks_arr[i]);
                                            }
                                        }
                                    }
                                    else if(cacheline.cache_state == INVALID)
                                    {
                                        for(int i=0;i<num_threads;i++)
                                        {
                                            if(i != thread_num)
                                            {
                                                omp_set_lock(&(locks_arr[i]));
                                                data_comm[i]->sender = thread_num;
                                                data_comm[i]->receiver = i;
                                                data_comm[i]->address = inst.address;
                                                data_comm[i]->value = inst.value;
                                                // data_comm[i]->cache_state = cacheline.cache_state;
                                                data_comm[i]->BusState = WRM;
                                                omp_unset_lock(&(locks_arr[i]));
                                            }
                                        }

                                    }
                                }
                                else
                                {
                                    //Write Miss
                                printf("inside write miss\n");

                                    for(int i=0;i<num_threads;i++)
                                    {
                                            if(i != thread_num)
                                            {
                                                omp_set_lock(&locks_arr[i]);
                                                //place INVALIDATE SIGNAL
                                                data_comm[i]->sender = thread_num;
                                                data_comm[i]->receiver = i;
                                                data_comm[i]->address = cacheline.address;
                                                data_comm[i]->value = cacheline.value;
                                                data_comm[i]->cache_state = cacheline.cache_state;
                                                data_comm[i]->BusState = WRM;
                                                omp_unset_lock(&locks_arr[i]);
                                            }
                                    }
                                    //Remember to give time gap
                                    cacheline.value = inst.value;
                                    //change the state to modified
                                    cacheline.cache_state = MODIFIED;
                                    // time gap
                                    for(int i=0;i<1000;i++);
                                    // copy from memory

                                    // cacheline.value = *(memory + inst.address);
                                    // write the data 
                                    
                                }
                                // printf("Writing to address %d: %d\n", cacheline.address, cacheline.value);
                                break;
                            }
                        }
                        *(c+hash) = cacheline;
                        print_cachelines(c,2,thread_num);
                        print("\n");
                        printf("Thread %d: %d %d: %d\n", thread_num,inst.type, inst.address, cacheline.value);
                        for(int i=0;i<1000;i++);
                    }   
                    instruction_finish = 1;
                    free(c);
                }
                #pragma omp section
                {
                    int read_no = 0;
                    int write_no = 0;
                    while(!instruction_finish){

                        if(data_comm[thread_num]!=NULL){
                            switch(data_comm[thread_num]->BusState)
                            {
                                case RDM:
                                {
                                    int hash = data_comm[thread_num]->address%cache_size;
                                    cache cacheline = *(c+hash);
                                    int to_resp = data_comm[thread_num]->sender;

                                    // If the data is in neighboring caches
                                    if(cacheline.address == data_comm[thread_num]->address){
                                    
                                        if(data_comm[to_resp]==NULL || (data_comm[to_resp]!=NULL && data_comm[to_resp]->BusState==NODATARDM))
                                        {
                                            if(cacheline.cache_state == MODIFIED){
                                                *(memory + cacheline.address) = cacheline.value;
                                            }
                                            cacheline.cache_state = SHARED;
                                            printf("changing to shared in thread:%d\n",thread_num);
                                            *(c+hash) = cacheline;
                                            omp_set_lock(&(locks_arr[to_resp]));
                                            data_comm[to_resp]->sender = thread_num;
                                            data_comm[to_resp]->receiver = to_resp;
                                            data_comm[to_resp]->address = cacheline.address;
                                            data_comm[to_resp]->value = cacheline.value;
                                            data_comm[to_resp]->BusState = DATARESP;
                                            data_comm[to_resp]->cache_state = SHARED;
                                            omp_unset_lock(&(locks_arr[to_resp]));
                                        }
                                    }
                                    else{
                                        if(data_comm[to_resp]==NULL || (data_comm[to_resp]!=NULL && data_comm[to_resp]->BusState==NODATARDM))
                                        {
                                            omp_set_lock(&locks_arr[to_resp]);
                                            data_comm[to_resp]->sender = thread_num;
                                            data_comm[to_resp]->receiver = to_resp;
                                            data_comm[to_resp]->address = data_comm[thread_num]->address;
                                            data_comm[to_resp]->BusState = NODATARDM;
                                            omp_set_lock(&locks_arr[to_resp]);
                                        }
                                    }
                                    // put at the end
                                    break;
                                }

                                case INVALIDATE: {
                                    int hash = data_comm[thread_num]->address%cache_size;
                                    cache cacheline = *(c+hash);
                                    if(cacheline.address == data_comm[thread_num]->address){
                                        cacheline.cache_state = INVALID;
                                    }
                                    *(c+hash) = cacheline;
                                    break;
                                }

                                case WRM : {
                                    int hash = data_comm[thread_num]->address%cache_size;
                                    cache cacheline = *(c+hash);
                                    int to_resp = data_comm[thread_num]->sender;
                                    if(cacheline.address != data_comm[thread_num]->address){
                                        if(data_comm[to_resp]==NULL || (data_comm[to_resp]!=NULL && data_comm[to_resp]->BusState==NODATAWRM))
                                        {
                                            omp_set_lock(&locks_arr[to_resp]);
                                            data_comm[to_resp]->sender = thread_num;
                                            data_comm[to_resp]->receiver = to_resp;
                                            data_comm[to_resp]->address = data_comm[thread_num]->address;
                                            data_comm[to_resp]->BusState = NODATAWRM;
                                            omp_set_lock(&locks_arr[to_resp]);
                                        }
                                    }
                                    else{
                                        if(cacheline.cache_state==SHARED || cacheline.cache_state==EXCLUSIVE){
                                            cacheline.cache_state = INVALID;
                                        }
                                        else if(cacheline.cache_state == MODIFIED)
                                        {
                                            *(memory + cacheline.address) = cacheline.value;
                                            cacheline.cache_state = INVALID;
                                        }
                                        printf("changing to invalid in thread %d\n",thread_num);
                                    }
                                    *(c+hash) = cacheline;

                                    break;

                                }
                                case DATARESP : {
                                    int hash = data_comm[thread_num]->address%cache_size;
                                    cache cacheline = *(c+hash);
                                    cacheline.value = data_comm[thread_num]->value;
                                    cacheline.value = SHARED;
                                    printf("changed to shared in thread = %d\n",thread_num);
                                    *(c+hash) = cacheline;
                                    read_no = 0;
                                    write_no = 0;
                                    break;
                                }
                                case NODATARDM : {
                                    // cases for no data in neighboring caches for RDM 
                                    if(read_no==3){
                                        int hash = data_comm[thread_num]->address%cache_size;
                                        cache cacheline = *(c+hash);
                                        cacheline.address = data_comm[thread_num]->address;
                                        cacheline.value = *(memory+data_comm[thread_num]->address);
                                        cacheline.cache_state = EXCLUSIVE;
                                        printf("changing to exclusive in thread %d\n",thread_num);
                                        read_no = 0;
                                    }
                                    else{
                                        read_no ++;
                                    }
                                    break;
                                }
                                case NODATAWRM : {
                                    if( write_no == 3){
                                        int hash = data_comm[thread_num]->address%cache_size;
                                        cache cacheline = *(c+hash);
                                        cacheline.address = data_comm[thread_num]->address;
                                        cacheline.value = *(memory+data_comm[thread_num]->address);
                                        cacheline.value = data_comm[thread_num]->value;
                                        cacheline.cache_state = MODIFIED;
                                        printf("changing to modified in thread %d\n",thread_num);
                                        write_no = 0;
                                    }
                                    else{
                                        write_no++;
                                    }
                                    break;
                                }
                            }
                            omp_set_lock(&locks_arr[thread_num]);
                            data_comm[thread_num] = NULL;
                            omp_unset_lock(&locks_arr[thread_num]);
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
   
    int NUM_THREAD = 2;
    cpu_loop(NUM_THREAD);
    free(memory);
}