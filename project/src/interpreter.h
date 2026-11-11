#include <stddef.h>
int interpreter(char *command_args[], int args_size);
int help();

struct queue* get_ready_queue();
struct PCB *run_pcb_to_completion(struct PCB *pcb);
struct PCB *run_pcb_for_n_steps(struct PCB *pcb, size_t n);