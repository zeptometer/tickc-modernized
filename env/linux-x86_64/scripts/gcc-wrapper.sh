#!/bin/sh
set -eu

link=1
conftest=0

for argument do
	case "$argument" in
		*conftest*)
			conftest=1
			;;
		-c|-S|-E)
			link=0
			;;
	esac
done

error_flags=
if [ "$conftest" = 0 ]; then
	error_flags="-Werror=implicit-int -Werror=implicit-function-declaration -Werror=int-conversion"
fi

# These variables are set only by the reproducible modern-Linux profiles.
# Intentional word splitting turns the controlled flag strings into arguments.
set -- "$@" ${TCC_HOST_CFLAGS:-}
if [ "$link" = 1 ]; then
	set -- "$@" ${TCC_HOST_LDFLAGS:-}
fi

exec /usr/bin/gcc \
	-std=gnu89 \
	-fPIE \
	$error_flags \
	"$@"
