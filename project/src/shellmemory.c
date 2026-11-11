#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "shellmemory.h"
#include "shell.h" // MAX_USER_INPUT
#include "queue.h"
#include "interpreter.h"
#include "pcb.h"


#define true 1
#define false 0


// global counter for LRU
static int time = 1;

// Helper functions
int match(char *model, char *var) {
    int i, len = strlen(var), matchCount = 0;
    for (i = 0; i < len; i++) {
        if (model[i] == var[i])
            matchCount++;
    }
    if (matchCount == len) {
        return 1;
    } else
        return 0;
}

struct frame {
    int allocated;
    int page_number;
    int last_used_time;
    char *name; // the program file this frame belongs to
    char *lines[LINES_PER_FRAME];
};

struct frame frame_store[FRAME_COUNT];

void update_time_frame(int frame_index) {
    frame_store[frame_index].last_used_time = ++time;
}

int replace_frame() {
    // LRU LOGIC HERE

    // find least recently used frame:
    int lru_frame_index = 0; // assume first one at first
    for (int i = 0; i < FRAME_COUNT; i++) {
        if (frame_store[i].allocated && 
            frame_store[i].last_used_time < frame_store[lru_frame_index].last_used_time) {
            lru_frame_index = i;
        }
    }

    // right now just remove random frame
    // random int between 0 and FRAME_COUNT - 1
    //int frame_nb = rand() % FRAME_COUNT; 
    struct frame *frame_to_remove = &frame_store[lru_frame_index];

    printf("Victim page contents:\n\n");
    for(int i = 0; i < LINES_PER_FRAME; i++) {
        if (frame_to_remove->lines[i] == NULL) {break;} // no instruction to display
        printf("%s\n", frame_to_remove->lines[i]);
    }
    printf("\nEnd of victim page contents.\n");

    // remove frame from pcb's page table
    struct PCB **victims_pcb = find_pcbs(get_ready_queue(), frame_to_remove->name);
    if (victims_pcb != NULL) { // how to handle if NULL, don;t really know...
        for (int i = 0; victims_pcb[i] != NULL; i++) {
            victims_pcb[i]->page_table[frame_to_remove->page_number] = -1;
        }
    }   

    free_frame(lru_frame_index);
    return lru_frame_index;
}

void init_frame_store() {
    for (size_t i = 0; i < FRAME_COUNT; ++i) {
        frame_store[i].allocated = false;
        for (size_t j = 0; j < LINES_PER_FRAME; ++j) {
            frame_store[i].lines[j] = NULL;
        }
    }
};

int get_free_frame() {
    for (int i = 0; i < FRAME_COUNT; ++i) {
        if (!frame_store[i].allocated) {
            frame_store[i].allocated = true;
            return i;
        }
    }

    return replace_frame(); // if no available frame, need to evict one
}

void free_frame(int frame_index) {
    if (frame_index >= 0 && frame_index < FRAME_COUNT) {
        frame_store[frame_index].allocated = false;
        // cleanup frame
        for (size_t i = 0; i < LINES_PER_FRAME; ++i) {
            if (frame_store[frame_index].lines[i] != NULL) {
                free(frame_store[frame_index].lines[i]);
                frame_store[frame_index].lines[i] = NULL;
            }
        }
        frame_store[frame_index].page_number = -1;
        frame_store[frame_index].last_used_time = -1;
        frame_store[frame_index].name = NULL;
    }
}

void load_line_to_frame(int frame_index, int offset, char *line) {
    frame_store[frame_index].lines[offset] = strdup(line);
}

char *mem_get_frame_line(int frame_index, int offset) {
    return frame_store[frame_index].lines[offset];
}

void enter_info_frame(int cur_frame, int page_nb, char *fname) {
    frame_store[cur_frame].allocated = true;
    frame_store[cur_frame].page_number = page_nb;
    frame_store[cur_frame].name = fname;
    update_time_frame(cur_frame);
}


// Shell memory functions

struct memory_struct { // block or line
    char *var;
    char *value;
};

struct memory_struct shellmemory[VAR_MEM_SIZE];



void mem_init() {
    int i;
    for (i = 0; i < VAR_MEM_SIZE; i++) {
        shellmemory[i].var = "none";
        shellmemory[i].value = "none";
    }
}

// Set key value pair
void mem_set_value(char *var_in, char *value_in) {
    int i;

    for (i = 0; i < VAR_MEM_SIZE; i++) {
        if (strcmp(shellmemory[i].var, var_in) == 0) {
            shellmemory[i].value = strdup(value_in);
            return;
        }
    }

    //Value does not exist, need to find a free spot.
    for (i = 0; i < VAR_MEM_SIZE; i++) {
        if (strcmp(shellmemory[i].var, "none") == 0) {
            shellmemory[i].var = strdup(var_in);
            shellmemory[i].value = strdup(value_in);
            return;
        }
    }

    return;
}

//get value based on input key
char *mem_get_value(char *var_in) {
    int i;

    for (i = 0; i < VAR_MEM_SIZE; i++) {
        if (strcmp(shellmemory[i].var, var_in) == 0) {
            return strdup(shellmemory[i].value);
        }
    }
    return NULL;
}
