#ifndef FIAL_LOADER_H
#define FIAL_LOADER_H

#include "interp.h"
#include "basic_types.h"  /*needed for map stuff*/


int FIAL_load_lookup (struct FIAL_interpreter *interp,
		     const char *lib_label,
		      union FIAL_lib_entry **ret_lib);

/*thesse are used to load libraries.  Generally best to use
  "load_string", the load_file is largely just a helper, but I've
  exposed it anyway, I guess in case you are positive that the file
  has not been loaded.  But even then, not much is saved with
  load_file.  Might rework this interface! */

int FIAL_load_string (struct FIAL_interpreter *interp,
		      const char *lib_label,
		      union FIAL_lib_entry **new_lib,
		      struct FIAL_error_info     *error);

int FIAL_load_file(struct FIAL_interpreter   *interp,
		   const char *filename,
		   union FIAL_lib_entry     **ret_lib,
		   struct FIAL_error_info     *error  );

int FIAL_load_c_lib (struct FIAL_interpreter    *interp,
		     char                       *lib_label,
		     struct FIAL_c_func_def     *lib_def,
		     struct FIAL_c_lib         **new_lib);

int FIAL_add_omni_lib (struct FIAL_interpreter *interp,
		       FIAL_symbol              sym,
		       struct FIAL_c_lib       *lib);

int FIAL_set_symbol_raw (struct FIAL_symbol_map *m, int sym,
			 struct FIAL_value const *val);

int FIAL_install_constants (struct FIAL_interpreter *);
int FIAL_install_std_omnis (struct FIAL_interpreter *);
int FIAL_install_system_lib (struct FIAL_interpreter *interp);


#endif /*FIAL_LOADER_H*/
