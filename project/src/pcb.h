#pragma once
#include <stddef.h>
#include <stdio.h> 
#include "shellmemory.h"
#include "shell.h" // VAR_MEM_SIZE and FRAME_STORE_SIZE

typedef size_t pid;

struct PCB {
    pid pid;
    int *page_table; //pointer to page table array
    int nb_of_pages;
    size_t line_count;
    size_t duration;
    size_t pc;
    char *name;
    struct PCB *next;
    int *ref_count;
};


int pcb_has_next_instruction(struct PCB *pcb);
char *pcb_next_instruction(struct PCB *pcb);
struct PCB *create_process(const char *filename);
struct PCB *create_process_from_FILE(FILE *script, const char *filename);
struct PCB *create_duplicate_process(struct PCB *pcb);
void free_pcb(struct PCB *pcb);

