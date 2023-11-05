#include "loader.h"
#include <signal.h>
#include <ucontext.h>

#define PAGE_SIZE 4096

// Header of ELF file
Elf32_Ehdr *ehdr;
// Program Header
Elf32_Phdr *phdr;
// File discriptor
int fd;
int num_page_fault = 0;
int num_page_allocations = 0;
int internal_fragmentation = 0;

struct node
{
    void *addr;
    void *next;
};

// head and tail of link list
struct node *head = NULL;
struct node *tail = NULL;

// link list is mainted to free the nodes using munmap
/*
 *add node to link list
 */
void add_node(void *addr)
{
    struct node *temp = malloc(sizeof(struct node));
    if (temp == NULL)
    {
        perror("Failed to allocate memory\n");
    }
    temp->next = NULL;
    temp->addr = addr;
    if (head == NULL)
    {
        head = temp;
    }
    if (tail == NULL)
    {
        tail = temp;
        return;
    }
    tail->next = temp;
    tail = temp;
}

// helper function
int math_ceil(int a, int b)
{
    if (b == 0)
        return 1;
    return (b / a + 1);
}

/*
 * release memory and other cleanups
 */
void loader_cleanup()
{
    int i = 0;
    while (head != NULL)
    {
        if (munmap(head->addr, PAGE_SIZE) == -1)
        {
            perror("Failed to free memory\n");
        }
        struct node *temp = head;
        head = head->next;
        free(temp);
        i++;
    }
    // printf("freed %d nodes\n",i);
    free(ehdr);
    close(fd);
}

/*
 * Check if the elf file is valid
 */
void check_elf(char **argv)
{
    Elf32_Ehdr *hdr = (Elf32_Ehdr *)malloc(sizeof(Elf32_Ehdr));
    if (hdr == NULL)
    {
        perror("Failed to alloacate memory\n");
    }

    fd = open(argv[0], O_RDONLY);
    if (fd == -1)
    {
        printf("Error in opening the file\n");
        exit(EXIT_FAILURE);
    }
    if (read(fd, hdr, sizeof(Elf32_Ehdr)) == -1)
    {
        perror("Error in reading the file\n");
    }
    close(fd);

    if (hdr->e_ident[EI_MAG0] != ELFMAG0)
    {
        printf("Error: Invalid ELF file\n");
        exit(1);
    }
    else if (hdr->e_ident[EI_MAG1] != ELFMAG1)
    {
        printf("Error: Invalid ELF file\n");
        exit(1);
    }
    else if (hdr->e_ident[EI_MAG2] != ELFMAG2)
    {
        printf("Error: Invalid ELF file\n");
        exit(1);
    }
    else if (hdr->e_ident[EI_MAG3] != ELFMAG3)
    {
        printf("Error: Invalid ELF file\n");
        exit(1);
    }
    if (hdr->e_ident[EI_DATA] != ELFDATA2LSB) // Assuming Little Endian System
    {
        printf("Error: Unsupported byte order\n");
        exit(1);
    }
    if (hdr->e_ident[EI_CLASS] == ELFCLASSNONE)
    {
        printf("Error: Invalid Class\n");
        exit(1);
    }
    if (hdr->e_ident[EI_VERSION] != EV_CURRENT)
    {
        printf("Error: Invalid version of ELF specification\n");
        exit(1);
    }
    if (hdr->e_version != EV_CURRENT)
    {
        printf("Error: Invalid File version\n");
        exit(1);
    }
    free(hdr);
}

/*
 * Load and run the ELF executable file
 */
void load_and_run_elf(char **argv)
{
    check_elf(argv);

    fd = open(argv[0], O_RDONLY);
    if (fd == -1)
    {
        perror("Error opening file\n");
    }

    int total_size = lseek(fd, 0, SEEK_END);
    if (total_size == -1)
    {
        perror("Error in lseek\n");
    }

    if (lseek(fd, 0, SEEK_SET) == -1)
    {
        perror("Error in lseek\n");
    }

    void *buffer = malloc(total_size);
    if (buffer == NULL)
    {
        perror("Failled to alloacate memory\n");
    }

    if (read(fd, buffer, total_size) == -1)
    {
        perror("Error in reading the file\n");
    }

    ehdr = (Elf32_Ehdr *)malloc(sizeof(Elf32_Ehdr));
    if (ehdr == NULL)
    {
        perror("Failed to allocate memory\n");
    }
    ehdr = (Elf32_Ehdr *)buffer;

    Elf32_Addr entry_pt_addr = (Elf32_Addr)ehdr->e_entry;

    typedef int (*fpp)();
    fpp _start = (fpp)(entry_pt_addr);

    // printf("Calling _start\n");
    int result = _start();
    printf("User _start return value = %d\n", result);
}

/*
 * Find the segment in which address lie in
 * allocate memory using mmap
 * copy the content using lseek and read
 */
void segmentation_handler(int signum, siginfo_t *info, void *no_use)
{
    num_page_fault++;
    int number_of_program = ehdr->e_phnum;
    int program_header_size = ehdr->e_phentsize;
    // Finding the segment in which the address lies
    for (int i = 0; i < number_of_program; i++)
    {
        phdr = (Elf32_Phdr *)((void *)ehdr + ehdr->e_phoff + i * program_header_size);
        if ((phdr->p_type == PT_LOAD) && ((phdr->p_vaddr <= (Elf32_Addr)(info->si_addr)) && ((Elf32_Addr)info->si_addr < (phdr->p_vaddr + phdr->p_memsz))))
        {
            break;
        }
    }

    int num_page = math_ceil(PAGE_SIZE, (int)phdr->p_memsz);
    int index = math_ceil(PAGE_SIZE, -(int)phdr->p_vaddr + (int)info->si_addr);
    num_page_allocations++;
    if (index == num_page)
        internal_fragmentation += num_page * PAGE_SIZE - phdr->p_memsz;
    index--;

    // allocating memory
    void *virtual_mem = mmap((void *)phdr->p_vaddr + index * PAGE_SIZE, PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
    if (virtual_mem == MAP_FAILED)
    {
        printf("Failed to allocate memory\n");
    }

    // adding node to link list to free the node
    add_node(virtual_mem);

    if (lseek(fd, phdr->p_offset , SEEK_SET) == -1)
    {
        perror("Failed to lseek\n");
    }

    if (read(fd, virtual_mem, PAGE_SIZE) == -1)
    {
        perror("Error in reading the file");
    }
    // memcpy(virtual_mem, (void *)ehdr + phdr->p_offset , PAGE_SIZE);
}

int main(int argc, char **argv)
{
    // for catching segmentation fault
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = segmentation_handler;
    sigaction(SIGSEGV, &sa, NULL);

    if (argc != 2)
    {
        printf("Usage: %s <ELF Executable> \n", argv[0]);
        exit(1);
    }

    load_and_run_elf(argv + 1);

    // printing the stats
    printf("Number of page fault %d\n", num_page_fault);
    printf("Number of page allocations %d\n", num_page_allocations);
    printf("Internal fragmentation %d\n", internal_fragmentation);

    // free and munmap
    loader_cleanup();
    return 0;
}