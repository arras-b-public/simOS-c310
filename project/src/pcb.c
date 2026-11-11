#include <stdio.h>
#include <stdlib.h>
#include <string.h> // memset
#include "shell.h" // MAX_USER_INPUT
#include "shellmemory.h"
#include "pcb.h"
#include <math.h> // ceil(x)

void pcb_load_next_page(struct PCB *pcb, int page_num) {
    int frame = get_free_frame();

    char linebuf[MAX_USER_INPUT];
    memset(linebuf, 0, sizeof(linebuf));
    FILE *program = fopen(pcb->name, "r");

    // skip all previous instructions
    for (int i = 0; i < pcb->pc; i++) {
        if (fgets(linebuf, MAX_USER_INPUT, program) == NULL) {
            break;
        }
    }   

    // load next 3 instruction into frame    
    for (int i = 0; i < LINES_PER_FRAME; i++) { 
        if (fgets(linebuf, MAX_USER_INPUT, program) == NULL) {
            break;
        }
        load_line_to_frame(frame, i, linebuf);
    }

    enter_info_frame(frame, page_num, pcb->name);
    pcb->page_table[page_num] = frame;
    fclose(program);
}

int pcb_has_next_instruction(struct PCB *pcb) {
    return pcb->pc < pcb->line_count;
}

char *pcb_next_instruction(struct PCB *pcb) {
    int page_num = pcb->pc / LINES_PER_FRAME;
    int offset = pcb->pc % LINES_PER_FRAME;
    int frame_num = pcb->page_table[page_num];

    //fprintf(stderr, "DEBUG: %s pc=%zu page=%d frame=%d\n", pcb->name, pcb->pc, page_num, frame_num);

    if (frame_num == -1) { // page is not loaded, need to load it into main memory
        printf("Page fault!\n");
        // load needed page into main memory
        pcb_load_next_page(pcb, page_num);
        return NULL;
    }

    pcb->pc++;
    update_time_frame(frame_num); // update frame since we just used it
    return mem_get_frame_line(frame_num, offset);
}

static pid fresh_pid = 1;


struct PCB *create_process(const char *filename) {
    FILE *script = fopen(filename, "rt");
    if (!script) {
        //perror("failed to open file for create_process");
        printf("CANNOT OPEN: %s", filename);
        return NULL;
    }
    struct PCB *pcb = create_process_from_FILE(script, filename);
    return pcb;
}

struct PCB *create_process_from_FILE(FILE *script, const char *filename) {

    // initilize struct fields:
    struct PCB *pcb = malloc(sizeof(struct PCB));
    pcb->pid = fresh_pid++;
    pcb->name = strdup(filename);
    pcb->next = NULL;
    pcb->pc = 0;
    pcb->ref_count = malloc(sizeof(int));
    *pcb->ref_count = 1;

    // find all lines:
    char linebuf[MAX_USER_INPUT];
    memset(linebuf, 0, sizeof(linebuf));
    int n = 0;

    while (fgets(linebuf, MAX_USER_INPUT, script) != NULL) {n++;}
    pcb->line_count = n;
    pcb->duration = n; 

    pcb->nb_of_pages = (int) ceil(pcb->line_count/ ((double) LINES_PER_FRAME));
    pcb->page_table = malloc(sizeof(int) * pcb->nb_of_pages);

    // Initialize page table with -1 (not in memory)
    for (int i = 0; i < pcb->nb_of_pages; i++) {
        pcb->page_table[i] = -1;
    }



    // load first 2 pages of program into memory:

    int cur_page = 0;
    int offset = 0;
    int cur_frame = -1;
    int page_loaded = 0;

    rewind(script); // go back to start of file

    while (fgets(linebuf, MAX_USER_INPUT, script) != NULL) {

        // only load the first 2 pages of a program (6 lines of code total)
        if (page_loaded >= 2) {
            break;
        }

        // Remove newline character
        linebuf[strcspn(linebuf, "\n")] = 0;
        
        // Check if we need a new frame
        if (offset == 0) {
            cur_frame = get_free_frame();
            pcb->page_table[cur_page] = cur_frame;
            cur_page++;
            offset = 0;
            update_time_frame(cur_frame);
        }

        load_line_to_frame(cur_frame, offset, linebuf);
        offset++;

        // if frame is full, need new one for next line
        if (offset == LINES_PER_FRAME) {
            offset = 0;
            enter_info_frame(cur_frame, page_loaded, pcb->name);
            page_loaded++;
        }
    }

    // first/second page is partially filled
    if (offset > 0) { 
        enter_info_frame(cur_frame, page_loaded, pcb->name);
    }

    fclose(script);
    return pcb;
}

struct PCB *create_duplicate_process(struct PCB *pcb) {
    struct PCB *new_pcb = malloc(sizeof(struct PCB));
    new_pcb->pid = fresh_pid++;
    //new_pcb->page_table = 
    new_pcb->nb_of_pages = pcb->nb_of_pages;
    new_pcb->line_count = pcb->line_count;  
    new_pcb->duration = pcb->duration;
    new_pcb->pc = 0;    
    new_pcb->name = strdup(pcb->name);
    new_pcb->next = NULL;
    new_pcb->ref_count = pcb->ref_count;
    (*new_pcb->ref_count)++;
    return new_pcb;
}
        

void free_pcb(struct PCB *pcb) {
    /* When a process terminates, you should NOT clean up its corresponding 
    pages in the frame store
    for (int i = 0; i < FRAME_COUNT; i++) {
        if (pcb->page_table[i] != -1) {
            free_frame(pcb->page_table[i]);
        }
    }
    */
    free(pcb->page_table);
    free(pcb->ref_count);
    free(pcb);
}
