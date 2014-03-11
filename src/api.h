#ifndef FIAL_API_H
#define FIAL_API_H

#include "interp.h"
#include "basic_types.h"

int FIAL_run_strings (const char *lib_label,
		      const char *proc_name,
		      struct FIAL_value  *args,
		      struct FIAL_exec_env *env);
int FIAL_run_proc (struct FIAL_ast_node  *proc,
		   struct FIAL_value     *args,
		   struct FIAL_exec_env  *env);


#endif /*FIAL_API_H*/
