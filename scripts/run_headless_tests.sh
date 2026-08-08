#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${BUILD_DIR:-$repo_root/build}"
godot_bin="${GODOT_BIN:-godot}"
test_addon_bin="$repo_root/test_project/addons/godot-box3d/bin"
godot_metadata_dir="$repo_root/test_project/.godot"
jobs="${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || printf '4')}"

export CMAKE_BUILD_PARALLEL_LEVEL="$jobs"
if [[ -z "${MAKEFLAGS:-}" ]]; then
	export MAKEFLAGS="-j$jobs"
fi

case "$(uname -s)" in
	Darwin)
		extension_filename="libgodot-box3d.dylib"
		;;
	*)
		extension_filename="libgodot-box3d.so"
		;;
esac
extension_library="$repo_root/bin/$extension_filename"

if [[ -f "$repo_root/.gitmodules" ]]; then
	git -C "$repo_root" submodule update --init --recursive
fi

cmake -S "$repo_root" -B "$build_dir"
cmake --build "$build_dir" --target godot-box3d --parallel "$jobs"

if [[ ! -f "$extension_library" ]]; then
	printf 'ERROR: Expected extension library was not built: %s\n' "$extension_library" >&2
	exit 1
fi

mkdir -p "$test_addon_bin" "$godot_metadata_dir"
ln -sf "$extension_library" "$test_addon_bin/$extension_filename"

# Running a script directly does not scan the project for GDExtensions, so register Box3D by
# hand. Only Box3D: the tests never exercise another backend, and a second GDExtension built
# against a different Godot version can abort the process before Box3D registers, failing every
# test for an unrelated reason. Other physics addons stay available in the editor.
printf '%s\n' 'res://addons/godot-box3d/godot-box3d.gdextension' \
	> "$godot_metadata_dir/extension_list.cfg"

backend_test="backend_activation_test.gd"
failed_tests=()
passed_tests=()

# Optional markdown table for $GITHUB_STEP_SUMMARY; skipped entirely when TEST_SUMMARY is unset.
write_summary() {
	if [[ -z "${TEST_SUMMARY:-}" ]]; then
		return
	fi
	local total=$(( ${#passed_tests[@]} + ${#failed_tests[@]} ))
	{
		if (( ${#failed_tests[@]} == 0 )); then
			printf '### Headless tests: %d/%d passed\n\n' "${#passed_tests[@]}" "$total"
		else
			printf '### Headless tests: %d of %d failed\n\n' "${#failed_tests[@]}" "$total"
		fi
		printf '| Result | Test |\n|---|---|\n'
		local test_script
		for test_script in "${failed_tests[@]}"; do
			printf '| FAIL | `%s` |\n' "$test_script"
		done
		for test_script in "${passed_tests[@]}"; do
			printf '| pass | `%s` |\n' "$test_script"
		done
	} > "$TEST_SUMMARY"
}

run_test() {
	local test_script="$1"
	local test_output
	local test_status=0

	printf '\n== %s ==\n' "$test_script"
	test_output="$("$godot_bin" --headless --path "$repo_root/test_project" --script "res://tests/$test_script" 2>&1)" || test_status=$?
	printf '%s\n' "$test_output"

	if (( test_status == 0 )) && [[ "$test_output" == *"RIDs in Godot Box3D were found to not have been freed"* ]]; then
		printf 'ERROR: %s leaked one or more Box3D RIDs.\n' "$test_script" >&2
		test_status=1
	fi

	if (( test_status == 0 )) && {
		[[ "$test_output" == *"ERROR:"* ]] ||
			[[ "$test_output" == *"SCRIPT ERROR:"* ]] ||
			[[ "$test_output" == *"RESULT: FAIL"* ]]
	}; then
		printf 'ERROR: %s emitted an unexpected error or failure marker.\n' "$test_script" >&2
		test_status=1
	fi

	if (( test_status == 0 )); then
		passed_tests+=("$test_script")
	else
		failed_tests+=("$test_script")
	fi
	return "$test_status"
}

# The backend gate is the one hard stop: if Box3D did not load, every later result is noise.
if ! run_test "$backend_test"; then
	printf 'ERROR: Box3D backend did not activate; skipping the rest of the suite.\n' >&2
	write_summary
	exit 1
fi

# Every other test runs even after a failure, so one break does not hide the rest.
for test_path in "$repo_root"/test_project/tests/*_test.gd; do
	test_script="${test_path##*/}"
	if [[ "$test_script" == "$backend_test" ]]; then
		continue
	fi
	run_test "$test_script" || true
done

write_summary

if (( ${#failed_tests[@]} > 0 )); then
	printf '\n%d of %d tests failed:\n' \
		"${#failed_tests[@]}" "$(( ${#failed_tests[@]} + ${#passed_tests[@]} ))" >&2
	printf '  %s\n' "${failed_tests[@]}" >&2
	exit 1
fi

printf '\nAll %d tests passed.\n' "${#passed_tests[@]}"
