#include <stdbool.h>


void swap_init();
void swap_free();
void swap_in(struct s_page*);
bool swap_out(struct s_page*);

struct block* swapBlock;
