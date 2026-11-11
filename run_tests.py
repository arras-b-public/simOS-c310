import os
import re
import subprocess
import sys
import difflib
import shutil
import argparse

from collections import Counter

# Track last compiled sizes to avoid unnecessary rebuilds
_last_framesize = None
_last_varmemsize = None


def parse_memory_sizes(result_path):
    """Parse Frame Store Size and Variable Store Size from the first line of a result file."""
    with open(result_path, 'r') as f:
        first_line = f.readline()
    m = re.match(r'Frame Store Size\s*=\s*(\d+);\s*Variable Store Size\s*=\s*(\d+)', first_line)
    if m:
        return int(m.group(1)), int(m.group(2))
    return None, None


def recompile_mysh(project_src, framesize, varmemsize):
    """Recompile mysh with the given frame store and variable memory sizes."""
    global _last_framesize, _last_varmemsize
    if framesize == _last_framesize and varmemsize == _last_varmemsize:
        return True  # already compiled with these sizes

    print(f"  Recompiling mysh with framesize={framesize} varmemsize={varmemsize}...")
    try:
        subprocess.run(['make', 'clean'], cwd=project_src,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        result = subprocess.run(
            ['make', 'mysh', f'framesize={framesize}', f'varmemsize={varmemsize}'],
            cwd=project_src, capture_output=True, text=True, timeout=30
        )
        if result.returncode != 0:
            print(f"  Compilation failed:\n{result.stderr}")
            return False
        _last_framesize = framesize
        _last_varmemsize = varmemsize
        return True
    except Exception as e:
        print(f"  Compilation error: {e}")
        return False


def normalize_output(output):
    """
    ignore "Shell version 1.5 created Dec 2025" & "Frame Store Size ..." so set1 & set2 tests still pass
    """
    lines = output.splitlines(keepends=True)
    result = []
    i = 0

    # Skip header line(s)
    while i < len(lines) and (
        "shell version" in lines[i].lower() or
        "frame store size" in lines[i].lower()
    ):
        i += 1

    while i < len(lines):
        # Strip all trailing whitespace (tabs, spaces, newlines)
        line = lines[i].rstrip()

        # If "Page fault!" is immediately followed by "Victim page contents:"
        if line == "Page fault!":
            next_line = lines[i+1].rstrip() if i+1 < len(lines) else ""
            if next_line.startswith("Victim page contents:"):
                # Merge into one line
                result.append("Page fault! " + next_line + '\n')
                i += 2  # skip the next line, already merged
                continue
            else:
                result.append("Page fault!\n")
        else:
            result.append(line + '\n')

        i += 1

    # Strip trailing blank lines
    out = ''.join(result)
    return out.rstrip('\n') + '\n' if out.strip() else ''


def compare_mt_output(actual, expected):
    """For multithreaded tests, only check that token counts match, not order."""
    def token_counts(text):
        lines = text.splitlines()
        return Counter(line for line in lines if line.strip())

    return token_counts(actual) == token_counts(expected)

def run_tests(categories):
    project_root = os.path.dirname(os.path.abspath(__file__))
    project_src = os.path.join(project_root, 'project', 'src')
    mysh_executable = os.path.join(project_root, 'project', 'src', 'mysh')

    if not os.path.exists(mysh_executable):
        print(f"Error: Executable not found at {mysh_executable}")
        return

    passed = 0
    failed = 0

    for cat in categories:
        test_cases_dir = os.path.join(project_root, 'test-cases', cat)
        
        if not os.path.exists(test_cases_dir):
            print(f"Warning: Category directory not found {test_cases_dir}")
            continue

        test_files = [f for f in os.listdir(test_cases_dir) 
                      if f.endswith('.txt') and not '_result' in f]  
        test_files.sort()

        if not test_files:
            continue

        print(f"\n--- Running tests for {cat} ---")

        for test_file in test_files:
            test_path = os.path.join(test_cases_dir, test_file)
            result_file = test_file.replace('.txt', '_result.txt')
            result_path = os.path.join(test_cases_dir, result_file)

            if not os.path.exists(result_path):
                print(f"Warning: No result file found for {test_file} (expected {result_file})")
                continue

            # For A3 tests, recompile with the correct memory sizes
            if cat == 'A3':
                framesize, varmemsize = parse_memory_sizes(result_path)
                if framesize is None or varmemsize is None:
                    print(f"Warning: Could not parse memory sizes from {result_file}")
                    continue
                if not recompile_mysh(project_src, framesize, varmemsize):
                    print(f"Skipping {test_file} due to compilation failure.")
                    failed += 1
                    continue

            print(f"Running {cat}/{test_file}...", end=' ')

            try:
                with open(test_path, 'r') as f:
                    # Run mysh with input redirection
                    process = subprocess.Popen(
                        [mysh_executable],
                        stdin=f,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE,
                        text=True,
                        cwd=test_cases_dir # Run from test-cases dir so relative file paths work
                    )
                    stdout, stderr = process.communicate(timeout=5)

                    # Read expected output
                    with open(result_path, 'r') as rf:
                        expected_output = rf.read()

                    # Check for alternative expected output
                    result2_file = test_file.replace('.txt', '_result2.txt')
                    result2_path = os.path.join(test_cases_dir, result2_file)
                    expected_output2 = None
                    if os.path.exists(result2_path):
                        with open(result2_path, 'r') as rf2:
                            expected_output2 = rf2.read()

                    # remove first line from both
                    stdout = normalize_output(stdout)
                    expected_output = normalize_output(expected_output)
                    if expected_output2 is not None:
                        expected_output2 = normalize_output(expected_output2)

                    # Compare output (ignoring potential trailing newlines differences if minor, 
                    # but mysh output seems to have specific newlines so strict comparison first)
                    is_mt = 'MT' in test_file
                    if is_mt:
                        passed_check = compare_mt_output(stdout, expected_output)
                        if not passed_check and expected_output2 is not None:
                            passed_check = compare_mt_output(stdout, expected_output2)
                    else:
                        passed_check = (stdout == expected_output)
                        if not passed_check and expected_output2 is not None:
                            passed_check = (stdout == expected_output2)

                    if passed_check:
                        print("PASS")
                        passed += 1
                    else:
                        print("FAIL")
                        failed += 1
                        #print("--- Expected ---")
                        #print(expected_output)
                        #print("--- Actual ---")
                        #print(stdout)
                        #print("--- Diff ---")
                        #diff = difflib.unified_diff(
                        # expected_output.splitlines(keepends=True),
                        # stdout.splitlines(keepends=True),
                        # fromfile='Expected',
                        # tofile='Actual',
                        #)
                        #print(''.join(diff))
                        #if stderr:
                        #    print("--- Stderr ---")
                        #    print(stderr)
            except subprocess.TimeoutExpired:
                print("TIMEOUT")
                failed += 1
            except Exception as e:
                print(f"ERROR: {e}")
                failed += 1

    print(f"\nSummary: {passed} passed, {failed} failed.")


if __name__ == "__main__":
        

    parser = argparse.ArgumentParser(description="Run mysh tests.")
    parser.add_argument("category", nargs="?", default="all", type=str.lower,
                        choices=["set1", "set2", "set3", "custom", "all"],
                        help="The category of tests to run (A1, A2, A3, Custom) or 'all' for all tests.")
    args = parser.parse_args()
    
    if args.category == "all":
        categories = ["A1", "A2", "A3", "Custom"]
    elif args.category == "custom":
        categories = ["Custom"]
    else:
        categories = [args.category.upper()]
        
    run_tests(categories)

    # Cleanup test directories
    for cat in categories:
        for folder in ["toto", "testdir", "test"]:
            path = os.path.join("test-cases", cat, folder)
            if os.path.exists(path):
                try:
                    shutil.rmtree(path)
                except Exception as e:
                    print(f"Failed to remove {path}: {e}")
