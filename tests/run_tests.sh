#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

echo "Building..."
make -s

echo "Running demo/test_identifiers.bas"
OUT=$(./basica demo/test_identifiers.bas 2>&1)
printf "%s\n" "$OUT"

if echo "$OUT" | grep -q "^5$"; then
  echo "Smoke test PASS: found A_B output 5"
else
  echo "Smoke test FAIL: expected output line '5' not found"
  exit 2
fi

TESTS=(
  # Arrays
  "tests/array_test"
  "tests/numeric_array"
  "tests/dim_edge_cases"
  "tests/multidim_bounds"
  "tests/erase"
  "tests/option_base_test"

  # File I/O
  "tests/filo_block"
  "tests/filo_random"
  "tests/putget"
  "tests/put_from_array"
  "tests/get_into_array"
  "tests/delete"
  "tests/delete_line"
  "tests/name_test"
  "tests/kill_wildcard"

  "tests/files"
  "tests/dir_ops"
  "tests/dir_ops_ext"
  "tests/shell_test"
  "tests/sound_play"

  # Strings
  "tests/asc_chr_beep"
  "tests/print_using"
  "tests/print_using_ext"
  "tests/tab_len"
  "tests/string_funcs"
  "tests/instr_val_str"
  "tests/string_cmp"
  "tests/more_string_funcs"
  "tests/string_numeric_funcs"
  "tests/input_multi"
  "tests/get_dollar"
  "tests/inkey_dollar"
  "tests/on_error_goto"
  "tests/environ_test"

  # Math and Data handling
  "tests/math_funcs"
  "tests/def_fn"
  "tests/peek_poke"
  "tests/random"
  "tests/data_read_restore"
  "tests/swap"
  "tests/swap_array"
  "tests/variable_suffixes"
  "tests/nested_for"
  "tests/nested_while"
  "tests/on_goto_nested"
  "tests/on_goto_gosub"

  # IF ELSE
  "tests/if_else_basic"
  "tests/if_else_no_else"
  "tests/if_else_multiple_statements"
  "tests/if_else_nested"
  "tests/if_else_string_vars"

  # Error Recovery
  "tests/syntax_error"
  "tests/return_error"

  # New Features
  "tests/lof_loc_test"
  "tests/files_redirect_test"
  "tests/shebang_test"
)

for t in "${TESTS[@]}"; do
  echo "Running $t.bas"
  # Prepare deterministic fixtures for tests that rely on filesystem timestamps
  if [ "$t" = "tests/files" ]; then
    printf "test\n" > tests/f.tmp
    # Set a fixed timestamp: 2026-06-10 09:01:16
    touch -t 202606100901.16 tests/f.tmp
  fi

  OUT=$(./basica "$t.bas" 2>&1)
  EXP_FILE="$t.expected"
  if [ ! -f "$EXP_FILE" ]; then
    echo "Expected file missing: $EXP_FILE"
    exit 2
  fi
  EXP=$(cat "$EXP_FILE")
  if [ "$OUT" = "$EXP" ]; then
    echo "$t PASS"
  else
    echo "$t FAIL"
    echo "--- expected ---"
    printf "%s\n" "$EXP"
    echo "--- actual ---"
    printf "%s\n" "$OUT"
    exit 2
  fi
done

echo "All tests PASS"

echo "Cleaning up temp files"
rm -rf test_dir_ext tests/filo_test.tmp tests/putget.tmp tests/putfrom.tmp tests/getinto.tmp tests/delete_test.tmp tests/kill_tmp1.tmp tests/kill_tmp2.tmp tests/input_multi.tmp

exit 0
