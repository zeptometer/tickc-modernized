#include <string.h>
#include <unistd.h>

char *cpp[] = { _TICKC_CPP, "-undef",
		"-D__x86_64__", "-D__linux__", "-D__unix__",
		"-D__TCC__", "-D__ELF__", "$1", "$2", "$3", 0 };

char *include[] = { "-I"_TICKC_INC"/tickc", 0 };

char *com[] = { _TICKC_COM, "-target=x86_64-linux", "$1", "$2", "$3", 0 };

char *as[] = { _TICKC_AS, "--64", "-o", "$3", "$1", "$2", 0 };

char *ld[] = { _TICKC_LINK, "-pie", "-Wl,-z,noexecstack",
	       "-o", "$3", "$1", "-L"_TICKC_LIB, "$2", "",
	       "-ltickc-rts", "-licode", "-lvcode", "", "-ltcsup", "-lm",
	       0 };

char *cc2[] = { _TICKC_CC2, "-std=gnu89", "-fPIE", "-g", "-O2", "-fno-builtin",
		"-Wno-incompatible-pointer-types",
		"-Wno-implicit-function-declaration",
		"-D__TCC__", "-D__CC2__",
		"-I"_TICKC_INC"/tickc", "-I"_TICKC_INC,
		"-include", _TICKC_INC"/tickc/tickc-env.h",
		"-imacros", _TICKC_INC"/tickc/tickc-macros.h",
		"-S", "$1", "$2", "-o", "$3", 0 };

int option(char *argument)
{
	if (strcmp(argument, "-g") == 0)
		return 1;
	if (strcmp(argument, "-p") == 0) {
		ld[12] = "-pg";
		return 1;
	}
	if (strcmp(argument, "-b") == 0 && access(_TICKC_BBX, R_OK) == 0) {
		ld[8] = _TICKC_BBX;
		return 1;
	}
	return 0;
}
