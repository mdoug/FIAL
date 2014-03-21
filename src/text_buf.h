#ifndef FIAL_TEXT_BUF_H
#define FIAL_TEXT_BUF_H

#include <stdlib.h>
#include "basic_types.h"

struct FIAL_text_buf  *FIAL_create_text_buf (void);
void FIAL_destroy_text_buf (struct FIAL_text_buf *buffy);
int FIAL_text_buf_reserve (struct FIAL_text_buf *buffy, size_t space);
int FIAL_text_buf_append_str (struct FIAL_text_buf *buffy, char *str);
int FIAL_text_buf_append_text_buf (struct FIAL_text_buf *buffy,
				   struct FIAL_text_buf *willow);
int FIAL_text_buf_append_value (struct FIAL_text_buf *buffy,
				struct FIAL_value     *val);
int FIAL_install_text_buf (struct FIAL_interpreter *interp);

#endif /*FIAL_TEXT_BUF_H*/
