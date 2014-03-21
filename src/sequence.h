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
int FIAL_install_seq (struct FIAL_interpreter *interp);

#endif /* FIAL_SEQUENCE_H */
