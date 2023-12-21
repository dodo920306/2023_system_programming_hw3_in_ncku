#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#define MAX_MEMORY_STORAGE 16384 /* 16KB (16 * 1024) */
#define HEADER_RECORD_SIZE 20 /**
                               * This includes:
                               *     1. First character 'H' for showing that it's the header.
                               *     2. 6 characters of program name.
                               *     3. 6 characters of hex-value of start address.
                               *     4. 6 characters of program size.
                               *     5. Terminal character '\0'.
                               */
#define TEXT_RECORD_SIZE 70 /**
                              * This includes:
                              *     1. First character 'T' for showing that it's the text.
                              *     2. 6 characters of program name.
                              *     3. 6 characters of hex-value of start address.
                              *     4. 6 characters of program size.
                              *     5. Terminal character '\0'.
                              */
#define MODIFICATION_RECORD_SIZE 10 /**
                                      * This includes:
                                      *     1. First character 'M' for showing that it's for modification.
                                      *     2. 6 characters of starting location.
                                      *     3. 2 characters of length.
                                      *     4. Terminal character '\0'.
                                      */
#define END_RECORD_SIZE 8 /**
                            * This includes:
                            *     1. First character 'E' for showing that it's for the end.
                            *     2. 6 characters of taddr.
                            *     3. Terminal character '\0'.
                            */
#define ADDR_SIZE 6
#define PAGE_SIZE 64
#define MIN_RELOCATION_POSITION 4656  /* 0x1230 */
#define MAX_RELOCATION_POSITION 24576 /* 0x6000 */


int address_n_transfer(char *hex, int n);

int address_n_transfer(char *hex, int n)
{
    int result, offset;
    char temp = *(hex + n);
    *(hex + n) = '\0';
    if (sscanf(hex, "%x%n", &result, &offset) != 1) {
        result = -1;
    }
    if (hex[offset] != '\0') {
        result = -1;
    }
    *(hex + n) = temp;
    return result;
}

int main()
{
    /* In case that the char type isn't 1 byte long, uint8_t is used here. */
    uint8_t *memory = NULL;
    char *input = NULL;
    int program_size = 0, saddr = 0, taddr = 0, raddr = 0, pages = 0;

    /* Check if header is valid. */
    input = (char *)malloc(HEADER_RECORD_SIZE);
    if (input) {
        input[HEADER_RECORD_SIZE - 1] = '\0';
        if (scanf("%[^\n]", input) != 1 || strlen(input) + 1 != HEADER_RECORD_SIZE || *input != 'H') {
            free(input);
            fprintf(stderr, "Error: header record doesn't exist or is in invalid format.\n");
            return -1;
        }
    }
    else {
        perror("Error: ");
        return -1;
    }

    /* Check if size is valid. */
    saddr = address_n_transfer(&input[7], ADDR_SIZE);
    program_size = address_n_transfer(&input[13], ADDR_SIZE);

    input[0] = 'I';
    input[7] = '\0';
    printf("%s", input);
    free(input);

    if (program_size < 0 || saddr < 0) {
        fprintf(stderr, "Error: failed to transfer address from hex string.\n");
        return -1;
    }
    if (program_size > MAX_MEMORY_STORAGE) {
        fprintf(stderr, "Error: the program is larger than entire memory. Its size must within %d bytes.\n", MAX_MEMORY_STORAGE);
        return -1;
    }

    /* Allocate the memory. */
    memory = (uint8_t *)malloc(MAX_MEMORY_STORAGE);
    if (!memory) {
        perror("Error: ");
        return -1;
    }
    for (int i = 0; i < MAX_MEMORY_STORAGE; i++) {
        /* Initialization */
        memory[i] = 255;
    }

    /* Relocation */
    srand(time(NULL));
    pages = (MAX_RELOCATION_POSITION - MIN_RELOCATION_POSITION) / PAGE_SIZE + 1; /* The last page which start at MAX_RELOCATION_POSITION isn't included, thus the plus 1. */
    raddr = ((rand() % pages) * PAGE_SIZE + MIN_RELOCATION_POSITION) - saddr; /* The difference between new and old addr. */

    /* Read texts */
    input = (char *)malloc(TEXT_RECORD_SIZE);
    if (input) {
        while (true) {
            input[TEXT_RECORD_SIZE - 1] = '\0';
            if (scanf("%s", input) != 1 || strlen(input) >= TEXT_RECORD_SIZE) {
                free(input); free(memory);
                fprintf(stderr, "Error: in invalid format.\n");
                return -1;
            }
            if (*input != 'T')
                break;
            int start = address_n_transfer(&input[1], ADDR_SIZE), length = address_n_transfer(&input[7], 2);
            if (length < 0 || start < 0) {
                free(input); free(memory);
                fprintf(stderr, "Error: failed to transfer address from hex string.\n");
                return -1;
            }
            for (int i = 0; i < length; i++) {
                int byte = address_n_transfer(&input[9 + i * 2], 2);
                if (byte < 0) {
                    free(input); free(memory);
                    fprintf(stderr, "Error: failed to transfer address from hex string.\n");
                    return -1;
                }
                memory[start + i] = byte;
            }
        }
    }
    else {
        free(memory);
        perror("Error: ");
        return -1;
    }

    /* Read m */
    bool first = true;
    while (true) {
        uint32_t bytes = 0;
        input[MODIFICATION_RECORD_SIZE - 1] = '\0';
        if (!first && (scanf("%s", input) != 1 || strlen(input) >= MODIFICATION_RECORD_SIZE)) { /* read in */
            /* Even e record won't longer than m record. */
            free(input); free(memory);
            fprintf(stderr, "Error: invalid format.\n");
            return -1;
        }
        first = false;
        if (*input != 'M')
            break;
        int start = address_n_transfer(&input[1], ADDR_SIZE);
        if (start < 0) {
            free(input); free(memory);
            fprintf(stderr, "Error: failed to transfer address from hex string.\n");
            return -1;
        }
        /* Make bytes = 0x00|memory[start ~ start + 2] */
        for (int i = 0; i < 3; i++) {
            bytes <<= 8;
            bytes += memory[start + i];
        }
        bytes &= 0xFFFFF; /* Make bytes = 0x000|last 4 bits of memory[start]|memory[start + 1 ~ start + 2] */
        bytes += raddr; /* relocation */
        /* Put bytes back into the memory. */
        memory[start] &= 0xF0; /* memory[start] = first 4 bits of itself */
        memory[start] += (bytes >> 16); /* memory[start] += 0x0000XXXX while 0xXXXX is the first 4 bits of the result */
        memory[start + 2] = (bytes & 0x000FF); /* memory[start + 2] = the last byte of the result */
        memory[start + 1] = ((bytes & 0x0FF00) >> 8); /* memory[start + 1] = the middle of the result */
    }

    /* Read e */
    input[END_RECORD_SIZE - 1] = '\0';
    if (*input != 'E') {
        free(input); free(memory);
        fprintf(stderr, "Error: invalid format.\n");
        return -1;
    }
    if (strlen(input) > 1) {
        taddr = address_n_transfer(&input[1], ADDR_SIZE);
    }
    else
        taddr = saddr;
    free(input);
    if (taddr < 0) {
        free(memory);
        fprintf(stderr, "Error: failed to transfer address from hex string.\n");
        return -1;
    }
    saddr += raddr;
    taddr += raddr;

    printf("%06X", saddr);
    printf("%06X", program_size);
    printf("%06X", taddr);
    for (int i = 0; i < program_size; i++) {
        if (!(i % 32))
            printf("\n");
        printf("%02X", memory[saddr + i]);
    }
    printf("\n");
    free(memory);
    return 0;
}
