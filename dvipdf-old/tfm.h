#ifndef TFM_H

#define TFM_H

#include "numbers.h"

void tfm_set_verbose(void);
void tfm_set_debug(void);

int tfm_open(char * tex_font_name);

double tfm_get_width (int font_id, UNSIGNED_PAIR ch);

#endif /* TFM_H */
