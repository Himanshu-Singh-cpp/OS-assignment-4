# OS-assignment-4

Link to the GitHub repository: https://github.com/Singh-Himanshu-cpp/OS-assignment-4 

In this assignment we have created a Simple Smart Loader. This loader loads program segments lazily, and not upfront.

We did this in the following way:

* a) First run the program using from e_entry
* b) Catch the SIGSEGV signal of the resulting Segmentation Fault
* c) Inside the signal handler, first find the address where segmentation fault occurred, using si_addr.
* d) Iterate through the Program Header Table and find the segment in which this address lies.
* e) Allocate the required memory for this segment in terms of pages of size 4096 bytes (4KB) using mmap
* f) Create a node of the reulting pointer and add it to linked list.
* g) Use memcpy to copy the segments contents to this location in virtual address space.
* h) Inside the loader_cleanup, do munmap for the all the address contained in the nodes.

In the Bonus:

Instead of one-shot allocation of a segment, we have done it page-by-page, treating the virtual memory of intra-segment
space to be continuous, but considering the fact that physical memory may/may not be continuous.

The 2 files we used for testing were:

1) fib.c : Calculates the 40th Fibonacci Number
2) sum.c

The results we got were:

a) Without Bonus:

   i) make ; ./loader fib:
      
      Number of page faults = 1
      Number of page allocations = 1
      Internal Fragmentation = 3982 bytes

   ii) make ; ./loader sum:

       Number of page faults = 2
       Number of page allocations = 4
       Internal Fragmentation = 8078 bytes

b) With Bonus:

   i) make ; ./loader fib:

      Number of page faults = 1
      Number of page allocations = 1
      Internal Fragmentation = 3982 bytes

   ii) make ; ./loader sum:

       Number of page faults = 3
       Number of page allocations = 3
       Internal Fragmentation = 8078 bytes


 Himanshu Singh : 2022217, 
 Vijval Ekbote : 2022569

We have both contributed equally to this assignment.
