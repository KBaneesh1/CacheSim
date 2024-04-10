#include <stdio.h>

// Define an enumeration named 'state'
enum state {
    a,
    b,
    c
}; // Don't forget the semicolon here

int main() {
    // Declare a variable 'l' of type 'state' and assign it the value 'a'
    enum state l = a;
    if(l==a){
        printf("hi");
    }
    // Print the integer value corresponding to 'state' (not recommended)
    printf("%d", l); // You should print the value of 'l', not 'state'
    
    return 0;
}
