#ifndef FIAL_SEQUENCE_H
#define FIAL_SEQUENCE_H

#include "basic_types.h"

struct FIAL_seq *FIAL_create_seq (void);
void FIAL_destroy_seq (struct FIAL_seq *seq,
		       struct FIAL_interpreter *interp);
int FIAL_seq_in (struct FIAL_seq *seq,  struct FIAL_value *val);
int FIAL_seq_first(struct FIAL_value *val,
		   struct FIAL_seq   *seq,
		   struct FIAL_interpreter *interp);
int FIAL_seq_last(struct FIAL_value *val,
		  struct FIAL_seq   *seq,
		  struct FIAL_interpreter *interp);
int FIAL_seq_copy (struct FIAL_seq *to,
		   struct FIAL_seq *from,
		   struct FIAL_interpreter *interp);
int FIAL_seq_append(struct FIAL_seq *to, struct FIAL_seq *from);
int FIAL_seq_reserve (struct FIAL_seq *seq, int new_size);
int FIAL_install_seq (struct FIAL_interpreter *interp);

#endif /* FIAL_SEQUENCE_H */
