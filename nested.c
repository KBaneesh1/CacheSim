#include <omp.h>
#include <stdio.h>

int main() {
    // Set nested parallelism
    omp_set_nested(1);

    #pragma omp parallel num_threads(4)
    {
        int outer_thread_id = omp_get_thread_num();
        printf("Outer parallel region, Thread ID: %d\n", outer_thread_id);

        #pragma omp parallel num_threads(2)
        {
            // printf("Inner parallel region, Parent Thread ID: %d, Thread ID: %d\n", outer_thread_id, inner_thread_id);

            #pragma omp sections
            {
                #pragma omp section
                {
                    int inner_thread_id = omp_get_thread_num();

                    printf("Section 1, Parent Thread ID: %d\n", outer_thread_id);
                }

                #pragma omp section
                {
                int inner_thread_id = omp_get_thread_num();

                    printf("Section 2, Parent Thread ID: %d\n", outer_thread_id);

                }
            }
        }
    }

    return 0;
}
