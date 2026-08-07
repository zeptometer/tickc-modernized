#!/bin/sh
set -eu

source_dir=/workspace
profile=${1:-debug}
work_dir="/tmp/tcc-modern-linux-$profile"
build_dir="$work_dir/build"
prefix="$work_dir/install"
cc="$source_dir/env/linux-x86_64/scripts/gcc-wrapper.sh"
build_mode=debug
valgrind_enabled=0

case "$profile" in
    debug)
        TCC_HOST_CFLAGS="-O0 -g3"
        TCC_HOST_LDFLAGS=
        ;;
    optimized)
        build_mode=optimize
        TCC_HOST_CFLAGS="-O2 -g -DNDEBUG"
        TCC_HOST_LDFLAGS=
        ;;
    sanitizer)
        TCC_HOST_CFLAGS="-O1 -g3 -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all"
        TCC_HOST_LDFLAGS="-fsanitize=address,undefined"
        ASAN_OPTIONS="detect_leaks=1:halt_on_error=1:strict_string_checks=1"
        UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1"
        export ASAN_OPTIONS UBSAN_OPTIONS
        ;;
    valgrind)
        TCC_HOST_CFLAGS="-O0 -g3"
        TCC_HOST_LDFLAGS=
        valgrind_enabled=1
        ;;
    *)
        echo "unknown quality profile: $profile" >&2
        exit 2
        ;;
esac
export TCC_HOST_CFLAGS TCC_HOST_LDFLAGS

run_logged()
{
    log=$1
    shift
    if "$@" >"$log" 2>&1; then
        return 0
    fi
    cat "$log"
    return 1
}

run_checked()
{
    if [ "$valgrind_enabled" = 1 ]; then
        valgrind --quiet \
            --error-exitcode=97 \
            --leak-check=full \
            --show-leak-kinds=definite \
            --errors-for-leak-kinds=definite \
            --track-origins=yes \
            "$@"
    else
        "$@"
    fi
}

dump_program_diagnostics()
{
    program=$1
    echo "Diagnostic metadata for $program" >&2
    file "$program" >&2 || true
    readelf -h "$program" >&2 || true
    objdump -drwC "$program" >"$program.disassembly" 2>&1 || true
    echo "First 240 lines of disassembly:" >&2
    sed -n '1,240p' "$program.disassembly" >&2 || true
}

run_test_program()
{
    program=$1
    shift
    if run_checked "$program" "$@"; then
        return 0
    fi
    dump_program_diagnostics "$program"
    return 1
}

run_expected_failure()
{
    stdout=$1
    stderr=$2
    shift 2
    if "$@" >"$stdout" 2>"$stderr"; then
        echo "expected command failure, but the command succeeded" >&2
        return 1
    else
        status=$?
    fi
    if [ "$valgrind_enabled" = 1 ] && [ "$status" = 97 ]; then
        cat "$stderr" >&2
        echo "Valgrind reported an error in an expected-failure test" >&2
        return 1
    fi
    return 0
}

assert_elf64_pie_nx()
{
    program=$1
    readelf -h "$program" | grep -q 'Class:.*ELF64'
    readelf -h "$program" | grep -q 'Type:.*DYN'
    stack_line=$(readelf -W -l "$program" | grep 'GNU_STACK')
    case "$stack_line" in
        *E*)
            echo "Executable stack found in $program" >&2
            exit 1
            ;;
    esac
}

rm -rf "$work_dir"
mkdir -p "$build_dir" "$prefix/build" "$prefix/bin" "$prefix/lib"

echo "Host, toolchain, and quality profile"
echo "$profile"
uname -m
gcc --version | sed -n '1p'
ld --version | sed -n '1p'
test "$(uname -m)" = x86_64
test "$(cat /proc/sys/kernel/randomize_va_space)" -gt 0

echo "Rejecting retired and unsupported targets"
mkdir -p "$work_dir/reject-i386" "$work_dir/reject-nonlinux"
if (cd "$work_dir/reject-i386" &&
    "$source_dir/configure" \
        --build=x86_64-pc-linux-gnu \
        --host=x86_64-pc-linux-gnu \
        --target=i386-pc-linux-gnu \
        --prefix="$work_dir/reject-i386/install") \
        >"$work_dir/reject-i386.log" 2>&1; then
    echo "The retired i386 target was unexpectedly accepted" >&2
    exit 1
fi
grep -q 'only x86_64 is maintained' "$work_dir/reject-i386.log"

if (cd "$work_dir/reject-nonlinux" &&
    "$source_dir/configure" \
        --build=x86_64-pc-linux-gnu \
        --host=x86_64-pc-linux-gnu \
        --target=x86_64-pc-solaris2 \
        --prefix="$work_dir/reject-nonlinux/install") \
        >"$work_dir/reject-nonlinux.log" 2>&1; then
    echo "A non-Linux target was unexpectedly accepted" >&2
    exit 1
fi
grep -q 'only Linux is maintained' "$work_dir/reject-nonlinux.log"

echo "Building x86-64 Tick-C against modern glibc"
cd "$build_dir"
run_logged "$work_dir/configure.log" \
    "$source_dir/configure" \
        --build=x86_64-pc-linux-gnu \
        --host=x86_64-pc-linux-gnu \
        --target=x86_64-pc-linux-gnu \
        --prefix="$prefix"
run_logged "$work_dir/build.log" \
    make MODE="$build_mode" CC="$cc"
assert_elf64_pie_nx "$prefix/bin/tcc"

echo "Checking the fixed C-to-C driver path"
compat_output="$work_dir/c-backend-compat"
run_logged "$compat_output.compile.log" run_checked \
    "$prefix/bin/tcc" -v -C -o "$compat_output" \
    "$source_dir/tst/phase1/paper/hello.tc"
grep -q -- '-target=x86_64-linux' "$compat_output.compile.log"
if grep -q -- '-target=c-x86_64' "$compat_output.compile.log"; then
    echo "The driver unexpectedly selected the legacy c-x86_64 alias" >&2
    exit 1
fi
assert_elf64_pie_nx "$compat_output"
run_test_program "$compat_output" >"$compat_output.stdout"
diff -u "$source_dir/tst/phase1/paper/hello.stdout" \
    "$compat_output.stdout"

echo "Checking the x86-64 encoder and SysV integer ABI"
for test_name in basic encoding; do
    "$cc" -std=gnu99 -Wall -Wextra -Werror -fPIE -pie \
        -Wl,-z,noexecstack \
        -I"$source_dir/src/vcode-x86_64" \
        -o "$work_dir/$test_name" \
        "$source_dir/tst/phase5/vcode-x86_64/$test_name.c" \
        "$source_dir/src/vcode-x86_64/vcode.c" \
        "$source_dir/src/vcode-x86_64/code-memory.c"
    assert_elf64_pie_nx "$work_dir/$test_name"
    run_test_program "$work_dir/$test_name"
done

echo "Checking Phase 6A direct-vcode integer and pointer boundaries"
"$cc" -std=gnu99 -Wall -Wextra -Werror -fPIE -pie \
    -Wl,-z,noexecstack \
    -I"$source_dir/src/vcode-x86_64" \
    -o "$work_dir/phase6a-vcode" \
    "$source_dir/tst/phase6a/vcode-x86_64/integer-pointer.c" \
    "$source_dir/src/vcode-x86_64/vcode.c" \
    "$source_dir/src/vcode-x86_64/code-memory.c"
assert_elf64_pie_nx "$work_dir/phase6a-vcode"
run_test_program "$work_dir/phase6a-vcode"

echo "Checking Phase 6B scalar SSE and floating-point SysV ABI"
"$cc" -std=gnu99 -Wall -Wextra -Werror -fPIE -pie \
    -Wl,-z,noexecstack \
    -I"$source_dir/src/vcode-x86_64" \
    -o "$work_dir/phase6b-vcode" \
    "$source_dir/tst/phase6b/vcode-x86_64/floating-abi.c" \
    "$source_dir/src/vcode-x86_64/vcode.c" \
    "$source_dir/src/vcode-x86_64/code-memory.c" \
    -lm
assert_elf64_pie_nx "$work_dir/phase6b-vcode"
run_test_program "$work_dir/phase6b-vcode"

echo "Checking Phase 6C variadic SysV ABI"
"$cc" -std=gnu99 -Wall -Wextra -Werror -fPIE -pie \
    -Wl,-z,noexecstack \
    -I"$source_dir/src/vcode-x86_64" \
    -o "$work_dir/phase6c-vcode" \
    "$source_dir/tst/phase6c/vcode-x86_64/variadic-abi.c" \
    "$source_dir/src/vcode-x86_64/vcode.c" \
    "$source_dir/src/vcode-x86_64/code-memory.c"
assert_elf64_pie_nx "$work_dir/phase6c-vcode"
run_test_program "$work_dir/phase6c-vcode"
"$cc" -std=gnu99 -Wall -Wextra -Werror -fPIE -pie \
    -Wl,-z,noexecstack \
    -I"$source_dir/src/vcode-x86_64" \
    -o "$work_dir/phase6c-unsupported-float" \
    "$source_dir/tst/phase6c/vcode-x86_64/unsupported-float.c" \
    "$source_dir/src/vcode-x86_64/vcode.c" \
    "$source_dir/src/vcode-x86_64/code-memory.c"
if ! run_expected_failure \
    "$work_dir/phase6c-unsupported-float.stdout" \
    "$work_dir/phase6c-unsupported-float.stderr" \
    run_checked "$work_dir/phase6c-unsupported-float"; then
    echo "unpromoted variadic float unexpectedly succeeded" >&2
    exit 1
fi
grep -q 'float is promoted to double' \
    "$work_dir/phase6c-unsupported-float.stderr"

echo "Checking Phase 6D aggregate SysV ABI"
"$cc" -std=gnu99 -Wall -Wextra -Werror -fPIE -pie \
    -Wl,-z,noexecstack \
    -I"$source_dir/src/vcode-x86_64" \
    -o "$work_dir/phase6d-vcode" \
    "$source_dir/tst/phase6d/vcode-x86_64/aggregate-abi.c" \
    "$source_dir/src/vcode-x86_64/vcode.c" \
    "$source_dir/src/vcode-x86_64/code-memory.c"
assert_elf64_pie_nx "$work_dir/phase6d-vcode"
run_test_program "$work_dir/phase6d-vcode"

echo "Checking paper-derived programs"
for source in "$source_dir"/tst/phase1/paper/*.tc; do
    base=${source##*/}
    name=${base%.tc}
    output="$work_dir/paper-$name"
    run_logged "$output.compile.log" run_checked \
        "$prefix/bin/tcc" -o "$output" "$source"
    assert_elf64_pie_nx "$output"
    if ! run_checked "$output" >"$output.stdout"; then
        dump_program_diagnostics "$output"
        exit 1
    fi
    diff -u "$source_dir/tst/phase1/paper/$name.stdout" "$output.stdout"
done

echo "Checking LP64 dynamic code and C interoperability"
for backend in icode vcode; do
    backend_option=
    if test "$backend" = vcode; then
        backend_option=-V
    fi
    for name in lp64 sysv-interop; do
        source="$source_dir/tst/phase5/tickc/$name.tc"
        output="$work_dir/$backend-$name"
        run_logged "$output.compile.log" run_checked \
            "$prefix/bin/tcc" $backend_option -o "$output" "$source"
        assert_elf64_pie_nx "$output"
        if ! run_checked "$output" >"$output.stdout"; then
            cat "$output.stdout"
            dump_program_diagnostics "$output"
            echo "$backend $name exited with failure" >&2
            exit 1
        fi
        diff -u "$source_dir/tst/phase5/tickc/$name.stdout" "$output.stdout"
    done
done

echo "Checking Phase 6A Tick-C integer and pointer boundaries"
"$cc" -std=gnu99 -Wall -Wextra -Werror -fPIE \
    -c -o "$work_dir/phase6a-reference.o" \
    "$source_dir/tst/phase6a/tickc/reference.c"
for backend in icode vcode; do
    backend_option=
    if test "$backend" = vcode; then
        backend_option=-V
    fi
    source="$source_dir/tst/phase6a/tickc/integer-pointer.tc"
    output="$work_dir/$backend-phase6a-integer-pointer"
    run_logged "$output.compile.log" run_checked \
        "$prefix/bin/tcc" $backend_option -o "$output" "$source" \
        "$work_dir/phase6a-reference.o"
    assert_elf64_pie_nx "$output"
    if ! run_checked "$output" >"$output.stdout"; then
        cat "$output.stdout"
        dump_program_diagnostics "$output"
        echo "$backend Phase 6A integer/pointer test exited with failure" >&2
        exit 1
    fi
    diff -u "$source_dir/tst/phase6a/tickc/integer-pointer.stdout" \
        "$output.stdout"
done

echo "Checking Phase 6A lifecycle failure diagnostics"
for backend in icode vcode; do
    backend_option=
    if test "$backend" = vcode; then
        backend_option=-V
    fi
    source="$source_dir/tst/phase6a/tickc/invalid-decompile.tc"
    output="$work_dir/$backend-phase6a-invalid-decompile"
    run_logged "$output.compile.log" run_checked \
        "$prefix/bin/tcc" $backend_option -o "$output" "$source"
    if ! run_expected_failure "$output.stdout" "$output.stderr" \
        run_checked "$output"; then
        echo "$backend invalid decompile unexpectedly succeeded" >&2
        exit 1
    fi
    grep -q 'not a dynamically generated function' "$output.stderr"
done

echo "Checking Phase 6B Tick-C floating-point ABI"
"$cc" -std=gnu99 -Wall -Wextra -Werror -fPIE \
    -c -o "$work_dir/phase6b-reference.o" \
    "$source_dir/tst/phase6b/tickc/reference.c"
for backend in icode vcode; do
    backend_option=
    if test "$backend" = vcode; then
        backend_option=-V
    fi
    source="$source_dir/tst/phase6b/tickc/floating-abi.tc"
    output="$work_dir/$backend-phase6b-floating-abi"
    run_logged "$output.compile.log" run_checked \
        "$prefix/bin/tcc" $backend_option -o "$output" "$source" \
        "$work_dir/phase6b-reference.o"
    assert_elf64_pie_nx "$output"
    if ! run_checked "$output" >"$output.stdout"; then
        cat "$output.stdout"
        dump_program_diagnostics "$output"
        echo "$backend Phase 6B floating-point test exited with failure" >&2
        exit 1
    fi
    diff -u "$source_dir/tst/phase6b/tickc/floating-abi.stdout" \
        "$output.stdout"
done

echo "Checking Phase 6C Tick-C variadic calls"
"$cc" -std=gnu99 -Wall -Wextra -Werror -fPIE \
    -c -o "$work_dir/phase6c-reference.o" \
    "$source_dir/tst/phase6c/tickc/reference.c"
for backend in icode vcode; do
    backend_option=
    if test "$backend" = vcode; then
        backend_option=-V
    fi
    source="$source_dir/tst/phase6c/tickc/variadic-calls.tc"
    output="$work_dir/$backend-phase6c-variadic-calls"
    run_logged "$output.compile.log" run_checked \
        "$prefix/bin/tcc" $backend_option -o "$output" "$source" \
        "$work_dir/phase6c-reference.o"
    assert_elf64_pie_nx "$output"
    if ! run_checked "$output" >"$output.stdout"; then
        cat "$output.stdout"
        dump_program_diagnostics "$output"
        echo "$backend Phase 6C variadic call test exited with failure" >&2
        exit 1
    fi
    diff -u "$source_dir/tst/phase6c/tickc/variadic-calls.stdout" \
        "$output.stdout"
done

echo "Checking Phase 6D Tick-C aggregate ABI"
"$cc" -std=gnu99 -Wall -Wextra -Werror -fPIE \
    -c -o "$work_dir/phase6d-reference.o" \
    "$source_dir/tst/phase6d/tickc/reference.c"
for backend in icode vcode; do
    backend_option=
    if test "$backend" = vcode; then
        backend_option=-V
    fi
    source="$source_dir/tst/phase6d/tickc/aggregate-abi.tc"
    output="$work_dir/$backend-phase6d-aggregate-abi"
    run_logged "$output.compile.log" run_checked \
        "$prefix/bin/tcc" $backend_option -o "$output" "$source" \
        "$work_dir/phase6d-reference.o"
    assert_elf64_pie_nx "$output"
    if ! run_checked "$output" >"$output.stdout"; then
        cat "$output.stdout"
        dump_program_diagnostics "$output"
        echo "$backend Phase 6D aggregate test exited with failure" >&2
        exit 1
    fi
    diff -u "$source_dir/tst/phase6d/tickc/aggregate-abi.stdout" \
        "$output.stdout"
done

echo "Checking W^X and ASLR for generated code"
: >"$work_dir/code-addresses"
for attempt in 1 2 3 4 5; do
    "$work_dir/basic" --address >>"$work_dir/code-addresses"
done
test "$(sort -u "$work_dir/code-addresses" | wc -l)" -gt 1

echo "Modern Linux x86-64 tests passed"
