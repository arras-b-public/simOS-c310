#include <stdio.h>
#define LINES_PER_FRAME 3
#define FRAME_COUNT (FRAME_STORE_SIZE / LINES_PER_FRAME)

void update_time_frame(int frame_index);
int replace_frame();
void init_frame_store();
int get_free_frame();
void free_frame(int frame_index);
char *mem_get_frame_line(int frame_index, int offset);
void load_line_to_frame(int frame_index, int offset, char *line);
void enter_info_frame(int cur_frame, int page_nb, char *fname);

void mem_init();
char *mem_get_value(char *var);
void mem_set_value(char *var, char *value);