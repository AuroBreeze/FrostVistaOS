#!/usr/bin/env bash

set -u -o pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJ_DIR="$(dirname "$SCRIPT_DIR")"
cd "$PROJ_DIR"

PASS=0
FAIL=0
SKIP=0
VERBOSE=0
FILTER=""
BOOT="bare"
RUN_TIMEOUT=30
STOP=0
CURRENT_PID=""

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
NC='\033[0m'

print_usage() {
	echo "Usage: $0 [-v] [-t test_name] [-b boot_method] [-T timeout_sec]"
	echo "  -v              Verbose: print recent run logs"
	echo "  -t <name>       Run only test_<name>.c"
	echo "  -b <method>     Boot method: bare (default) or opensbi"
	echo "  -T <seconds>    Timeout per test run (default: 30)"
}

print_summary() {
	echo ""
	echo -e "${CYAN}Summary:${NC}  ${GREEN}$PASS passed${NC},  ${RED}$FAIL failed${NC},  ${YELLOW}$SKIP skipped${NC}"
}

on_interrupt() {
	STOP=1
	echo ""
	echo -e "${YELLOW}Interrupted (Ctrl+C), stopping...${NC}"
	if [ -n "$CURRENT_PID" ]; then
		kill -INT -- "-$CURRENT_PID" 2>/dev/null || true
	fi
}

trap on_interrupt INT

while getopts "vht:b:T:" opt; do
	case "$opt" in
		v) VERBOSE=1 ;;
		t) FILTER="$OPTARG" ;;
		b) BOOT="$OPTARG" ;;
		T) RUN_TIMEOUT="$OPTARG" ;;
		h)
			print_usage
			exit 0
			;;
		*)
			print_usage
			exit 1
			;;
	esac
done

if [ "$BOOT" != "bare" ] && [ "$BOOT" != "opensbi" ]; then
	echo "Invalid boot method: $BOOT"
	exit 1
fi

mapfile -t TESTS < <(
	find test -maxdepth 1 -name 'test_*.c' | sort | while read -r f; do
		basename "$f" .c | sed 's/^test_//'
	done
)

if [ -n "$FILTER" ]; then
	TESTS=("$FILTER")
fi

if [ ${#TESTS[@]} -eq 0 ]; then
	echo "No tests found."
	exit 1
fi

echo -e "${CYAN}Running ${#TESTS[@]} test(s) [boot: ${BOOT}] [timeout: ${RUN_TIMEOUT}s]${NC}"
echo ""

for test_name in "${TESTS[@]}"; do
	if [ "$STOP" -eq 1 ]; then
		break
	fi

	printf "  %-18s " "$test_name"
	RUN_LOG=$(mktemp)

	setsid timeout --foreground "${RUN_TIMEOUT}s" \
		make qemu BOOT="$BOOT" FS_LIST="easyfs devtmpfs" ROOTFS=easyfs TEST="$test_name" \
		>"$RUN_LOG" 2>&1 &
	CURRENT_PID=$!

	if wait "$CURRENT_PID"; then
		MAKE_RC=0
	else
		MAKE_RC=$?
	fi
	CURRENT_PID=""

	CLEAN_OUT=$(tr -d '\0' <"$RUN_LOG" | sed 's/\x1b\[[0-9;]*m//g')

	if [ "$STOP" -eq 1 ] || [ "$MAKE_RC" -eq 130 ]; then
		echo -e "[${YELLOW}INTERRUPTED${NC}]"
		rm -f "$RUN_LOG"
		break
	elif [ "$MAKE_RC" -eq 124 ]; then
		echo -e "[${YELLOW}TIMEOUT${NC}]"
		((SKIP++)) || true
	elif [ "$MAKE_RC" -ne 0 ]; then
		echo -e "[${RED}MAKE_FAIL${NC}]"
		((FAIL++)) || true
	elif echo "$CLEAN_OUT" | grep -qE '\bpanic\b|PANIC'; then
		echo -e "[${RED}PANIC${NC}]"
		((FAIL++)) || true
	elif ! echo "$CLEAN_OUT" | grep -q 'FrostVistaOS booting'; then
		echo -e "[${YELLOW}NO_BOOT_LOG${NC}]"
		((SKIP++)) || true
	else
		echo -e "[${GREEN}PASS${NC}]"
		((PASS++)) || true
	fi

	if [ "$VERBOSE" -eq 1 ]; then
		echo "$CLEAN_OUT" | tail -30 | sed 's/^/        /'
	fi

	rm -f "$RUN_LOG"
done

print_summary

if [ "$STOP" -eq 1 ]; then
	exit 130
fi

if [ "$FAIL" -gt 0 ]; then
	exit 1
fi
