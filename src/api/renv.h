#include <stdlib.h>

/* FIXME: does not seem to work! */
void setEnvironmentVariablesIfUnset(void) {
	#define OVERWRITE 1
	if (getenv("R_HOME") == NULL) {
		setenv("R_HOME", "", OVERWRITE);
	}

	if (getenv("LD_LIBRARY_PATH") == NULL) {
		setenv("LD_LIBRARY_PATH", "/usr/include/R:/lib:/lib/jvm/jre/lib/server", OVERWRITE);
	}

	if (getenv("R_SHARE_DIR") == NULL) {
		setenv("R_SHARE_DIR", "", OVERWRITE);
	}

	if (getenv("R_SHARE_DIR") == NULL) {
		setenv("R_DOC_DIR", "", OVERWRITE);
	}
	#undef OVERWRITE
}
