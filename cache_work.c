#include<stdlib.h>
#include<stdio.h>
#include<omp.h>
#include<string.h>
#include<ctype.h>
// #include<mtasker.h>
// #include<time>
typedef char byte;

enum cache_state{
    MODIFIED,
    EXCLUSIVE,
    SHARED,
    INVALID
};

struct cache {
    int address; // This is the address in memory.
    int value; // This is the value stored in cached memory.
    // State for you to implement MESI protocol.
    enum cache_state cache_state;
};

struct decoded_inst {
    int type; // 0 is RD, 1 is WR
    int address;
    int value; // Only used for WR 
};
enum BusState{
    RDM,
    WRM,
    RWITM,
    INVALIDATE,
    DATARESP,
    NODATARDM
    // NODATAWRM
};
struct Bus{
    int sender;
    int receiver;
    enum BusState BusState;
    int address;
    int value;
    int done;
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
void print_cachelines(cache * c, int cache_size,int thread_in){
    for(int i = 0; i < cache_size; i++){
        cache cacheline = *(c+i);
        printf("Thread:%d, Address: %d, State: %d, Value: %d\n", thread_in,cacheline.address, cacheline.cache_state, cacheline.value);
    }
}


// This function implements the mock CPU loop that reads and writes data.
void cpu_loop(int num_threads){
    // Initialize a CPU level cache that holds about 2 bytes of data.
    int memory_size = 24;
    memory = (byte *) calloc(memory_size,sizeof(byte));
    omp_lock_t locks_arr[num_threads];
    Bus *data_comm = (Bus*)calloc(num_threads,sizeof(Bus));
    for(int i=0;i<num_threads;i++){
        data_comm[i].done = 0;
        data_comm[i].address = 0;
        data_comm[i].value = 0;
        data_comm[i].sender = 0;
        data_comm[i].receiver = 0;
    }
    
    for (int i=0; i<num_threads; i++)
        omp_init_lock(&(locks_arr[i]));
    omp_set_nested(1);
    int instruction_finish = 0;

    #pragma omp parallel num_threads(num_threads) shared(memory,data_comm,locks_arr)
    {
        int cache_size = 2;
        cache * c = (cache *) calloc(cache_size,sizeof(cache));
        int thread_num = omp_get_thread_num();
        // Read Input file
        for(int i=0;i<cache_size;i++){
            c[i].address = 0;
            c[i].value = 0;
            c[i].cache_state = INVALID;
        }
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
                     
                        switch(inst.type)
                        {
                            case 0:{
                            // printf("inside read hit\n");
                            //Read Hit
                            if(cacheline.address == inst.address)
                            {
                                //No change while reading from M,S,EX
                                if(cacheline.cache_state == MODIFIED || cacheline.cache_state == SHARED || cacheline.cache_state == EXCLUSIVE)
                                    // printf("Reading from address %d: %d\n", cacheline.address, cacheline.value);
                                    break;
                                else{
                                    //INVALID READ HIT
                                    // SAME AS READ MISS
                                    printf("Read Hit Invalidate thread%d , addr:%d , value:%d\n",thread_num,inst.address,inst.value);
                                    for(int i=0;i<num_threads;i++)
                                    {
                                        if(i != thread_num)
                                        {
                                            // Bus temp_bus = (Bus*)malloc(sizeof(Bus));
                                            omp_set_lock(&(locks_arr[i]));
                                            data_comm[i].sender = thread_num;
                                            data_comm[i].receiver = i;
                                            data_comm[i].BusState = RDM;
                                            data_comm[i].address = inst.address;
                                            data_comm[i].value = inst.value;
                                            data_comm[i].done = 1;
                                            // data_comm[i] = temp_bus; 
                                            omp_unset_lock(&(locks_arr[i]));
                                        }
                                    }
                                }  
                                // break;     
                            }
                            else{
                                // Read Miss
                                printf("inside read miss of thread:%d, addr:%d , value:%d\n", thread_num,inst.address,inst.value);

                                for(int i=0;i<num_threads;i++)
                                {
                                    if(i != thread_num)
                                    {
                                      
                                      omp_set_lock(&(locks_arr[i]));
                                      data_comm[i].sender = thread_num;
                                      data_comm[i].receiver = i;
                                      data_comm[i].BusState = RDM;
                                      data_comm[i].address = inst.address;
                                      data_comm[i].value = -1;
                                      data_comm[i].done = 1;
                                    //   data_comm[i] = temp_bus; 
                                      omp_unset_lock(&(locks_arr[i]));
                                    }
                                }

                                // break;
                            
                                }
                                break;
                            }
                            case 1:
                            {
                                //Write Hit

                                if(cacheline.address == inst.address)
                                {
                                  printf("inside write hit ,thread:%d , addr:%d , value:%d\n",thread_num,inst.address,inst.value);

                                    if(cacheline.cache_state == EXCLUSIVE)
                                    {
                                        cacheline.cache_state = MODIFIED;
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
                                            //   Bus *temp_bus = (Bus*)malloc(sizeof(Bus));
                                              omp_set_lock(&(locks_arr[i]));
                                              data_comm[i].sender = thread_num;
                                              data_comm[i].receiver = i;
                                              data_comm[i].BusState = INVALIDATE;
                                              data_comm[i].address = cacheline.address;
                                              data_comm[i].value = cacheline.value;
                                              data_comm[i].done = 1;
                                            //   data_comm[i] = temp_bus; 
                                              omp_unset_lock(&(locks_arr[i]));
                                            }
                                        }
                                    }
                                    else if(cacheline.cache_state == INVALID)
                                    {
                                        printf("inside write hit invalidate, thread:%d addr:%d , value:%d\n",thread_num,inst.address,inst.value);
                                        
                                        cacheline.address = inst.address;
                                        cacheline.value = inst.value;
                                        cacheline.cache_state = MODIFIED;
                                      
                                        for(int i=0;i<num_threads;i++)
                                        {
                                            if(i != thread_num)
                                            {
                                            //   Bus *temp_bus = (Bus*)malloc(sizeof(Bus));
                                              omp_set_lock(&(locks_arr[i]));
                                              data_comm[i].sender = thread_num;
                                              data_comm[i].receiver = i;
                                              data_comm[i].BusState = INVALIDATE;
                                              data_comm[i].address = cacheline.address;
                                              data_comm[i].value = cacheline.value;
                                              data_comm[i].done = 1;
                                            //   data_comm[i] = temp_bus; 
                                              omp_unset_lock(&(locks_arr[i]));
                                            }
                                        }

                                    }
                                    *(c+hash) = cacheline;
                                }
                                else
                                {
                                    //Write Miss
                                  printf("inside write miss  thread:%d , addr : %d , value:%d \n",thread_num,inst.address,inst.value);
                                  if(cacheline.cache_state!=INVALID)
                                        *(memory+cacheline.address) = cacheline.value;

                                  cacheline.address = inst.address;
                                  cacheline.value = inst.value;
                                  //change the state to modified
                                  cacheline.cache_state = MODIFIED;
                                  // printf("inst address = %d\n",inst.address);
                                  // print_cachelines(c, 2);
                                    *(c+hash) = cacheline;

                                    for(int i=0;i<num_threads;i++)
                                    {
                                           
                                            if(i != thread_num)
                                            {
                                            //   Bus *temp_bus =(Bus*)malloc(sizeof(Bus));
                                              omp_set_lock(&(locks_arr[i]));
                                              data_comm[i].sender = thread_num;
                                              data_comm[i].receiver = i;
                                              data_comm[i].BusState = INVALIDATE;
                                              data_comm[i].address = inst.address;
                                              data_comm[i].value = inst.value;
                                              data_comm[i].done = 1;
                                            //   data_comm[i] = temp_bus;
                                              omp_unset_lock(&(locks_arr[i]));
                                            }
                                      // printf("ending lock\n");
                                    }
                                    
                                    //Remember to give time gap
                                    // time gap
                                    // copy from memory


                                }
                                
                                break;
                            }

                        }
                        for(int i=0;i<100000;i++);
                        // *(c+hash) = cacheline;
                        printf("Thread %d: %s %d: %d State = %d\n", thread_num,inst.type?"WR" 
 :"RD", inst.address, cacheline.value,cacheline.cache_state);
                        printf("--\n");
                        print_cachelines(c,cache_size,thread_num);
                        printf("--\n");
                        for(int i=0;i<1000;i++);
                    }
                    // #pragma omp barrier
                    instruction_finish++;
                    printf("inst finish = %d\n",instruction_finish);
                    
                }
                #pragma omp section
                {
                    int read_no = 0;
                    // int write_no = 0;
                    while(instruction_finish<2){

                        if(data_comm[thread_num].done){
                            omp_set_lock(&locks_arr[thread_num]);
                            switch(data_comm[thread_num].BusState)
                            {
                                case RDM:
                                {
                                    int hash = data_comm[thread_num].address%cache_size;
                                    cache line_cache = *(c+hash);
                                    int to_resp = data_comm[thread_num].sender;

                                    // If the data is in neighboring caches
                                    if(line_cache.address == data_comm[thread_num].address && line_cache.cache_state!=INVALID){
                                        //
                                        if(data_comm[to_resp].done==0 || (data_comm[to_resp].done==1 && data_comm[to_resp].BusState==NODATARDM))
                                        {
                                            // if(line_cache.cache_state == MODIFIED){
                                            //     *(memory + line_cache.address) = line_cache.value;
                                            // }
                                            printf("Data found in cache in 2nd thread\n");
                                            line_cache.cache_state = SHARED;
                                            *(c+hash) = line_cache;
                                            // Bus *temp_bus = (Bus*)malloc(sizeof(Bus));
                                            omp_set_lock(&(locks_arr[to_resp]));
                                            data_comm[to_resp].sender = thread_num;
                                            data_comm[to_resp].receiver = to_resp;
                                            data_comm[to_resp].BusState = DATARESP;
                                            data_comm[to_resp].address = line_cache.address;
                                            data_comm[to_resp].value = line_cache.value;
                                            data_comm[to_resp].BusState = SHARED;
                                            data_comm[to_resp].done = 1;
                                            // data_comm[to_resp] = temp_bus;
                                            omp_unset_lock(&(locks_arr[to_resp]));
                                        }
                                        printf("Inside  read miss and neighbors have data\n");
                                        print_cachelines(c,2,thread_num);


                                    }
                                    else{
                                        if(data_comm[to_resp].done==0 || (data_comm[to_resp].done==1 && data_comm[to_resp].BusState==NODATARDM))
                                        {
                                            printf("Data not found in neighboring caches\n");
                                        //   Bus *temp_bus = (Bus*)malloc(sizeof(Bus));
                                          omp_set_lock(&(locks_arr[to_resp]));
                                          data_comm[to_resp].sender = thread_num;
                                          data_comm[to_resp].receiver = to_resp;
                                          data_comm[to_resp].BusState = NODATARDM;
                                          data_comm[to_resp].address = data_comm[thread_num].address;
                                          data_comm[to_resp].done = 1;
                                        //   data_comm[to_resp] = temp_bus;
                                          omp_unset_lock(&(locks_arr[to_resp]));
                                        printf("sending Not Found data RDM from %d to %d\n", thread_num, to_resp);
                                        print_cachelines(c,2,thread_num);
                                        }

                                    }
                                    // put at the end
                                    break;
                                }

                                case INVALIDATE: {
                                    //
                                    int hash = data_comm[thread_num].address%cache_size;
                                    cache line_cache = *(c+hash);
                                    if(line_cache.address == data_comm[thread_num].address){
                                        
                                        line_cache.cache_state = INVALID;
                                    }
                                    *(c+hash) = line_cache;
                                    printf("Invalid in bus\n");
                                    print_cachelines(c,2,thread_num);
                                    break;
                                    
                                }

                                case DATARESP : {
                                    //
                                    int hash = data_comm[thread_num].address%cache_size;
                                    cache line_cache = *(c+hash);
                                    if(line_cache.address!=data_comm[thread_num].address){
                                        if(line_cache.cache_state!=INVALID){
                                            *(memory+line_cache.address) = line_cache.value;
                                        }
                                    }
                                    line_cache.address = data_comm[thread_num].address;
                                    line_cache.value = data_comm[thread_num].value;
                                    line_cache.value = SHARED;
                                    *(c+hash) = line_cache;
                                    read_no = 0;
                                    printf("received data response for thread %d\n",thread_num);
                                    print_cachelines(c,2,thread_num);
                                    //
                                    break;
                                }
                                case NODATARDM : {
                                    // cases for no data in neighboring caches for RDM 
                                    read_no++;
                                    printf("In No DATA\n");
                                    print_cachelines(c,2,thread_num);
                                    // printf("Inisde NORDM and read_no = %d\n",read_no);
                                    if(read_no>=1){
                                        // printf("inside no data RDM\n");
                                        //
                                        // print_cachelines(c,2,thread_num);
                                        printf("in NODATARDM , cache addr:%d , value = %d\n , memory = %d\n"    ,data_comm[thread_num].address,data_comm[thread_num].value,*(memory+data_comm[thread_num].address));
                                        int hash = data_comm[thread_num].address%cache_size;
                                        cache line_cache = *(c+hash);
                                        // printf("cache line , address = %d , value = %d , state = %d\n",line_cache.address,line_cache.value,line_cache.cache_state);
                                        // printf("checkpoint 1\n");
                                        if(line_cache.address!=0)
                                        {
                                            if(line_cache.cache_state!=INVALID)
                                                *(memory+line_cache.address) = line_cache.value;
                                        }
                                        line_cache.address = data_comm[thread_num].address;
                                        // printf("checkpoint 2\n");
                                        line_cache.value = *(memory+data_comm[thread_num].address);

                                        // printf("memory in = %d and value = %d\n",data_comm[thread_num].address,*(memory+data_comm[thread_num].address));
                                        line_cache.cache_state = EXCLUSIVE;
                                        *(c+hash) = line_cache;
                                        read_no = 0;
                                        printf("No data received\n");
                                        print_cachelines(c,2,thread_num);
                                        // omp_unset_lock(&(locks_arr[thread_num]));
                                    }
                                    
                                    break;
                                }
                                
                            }
                            // omp_set_lock(&locks_arr[thread_num]);
                            data_comm[thread_num].done = 0;
                            // omp_unset_lock(&locks_arr[thread_num]);
                        // print_line_caches(c, cache_size,thread_num);
                        // printf("\n");
                        omp_unset_lock(&locks_arr[thread_num]);
                        }

                    }
                }
            }
        }
        #pragma omp barrier
        printf("----------------------------------------\n");
        print_cachelines(c,2,thread_num);
        free(c);

    }
}

int main(int c, char * argv[]){
    // Initialize Global memory
    // Let's assume the memory module holds about 24 bytes of data.

    int NUM_THREAD = 2;
    cpu_loop(NUM_THREAD);
    free(memory);
}