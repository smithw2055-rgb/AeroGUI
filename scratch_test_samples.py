import os
import sys
import subprocess
import glob
import time

def main():
    build_samples_dir = os.path.abspath("build/samples")
    exe_pattern = os.path.join(build_samples_dir, "*", "Debug", "*.exe")
    executables = sorted(glob.glob(exe_pattern))
    
    print(f"Found {len(executables)} sample executables in {build_samples_dir}\n")
    
    results = []
    
    for exe in executables:
        name = os.path.basename(exe)
        working_dir = os.path.dirname(exe)
        print(f"==================================================")
        print(f"Testing Sample: {name}")
        print(f"Path: {exe}")
        print(f"Working Dir: {working_dir}")
        
        # Test 1: with --verify
        start_time = time.time()
        proc = None
        try:
            proc = subprocess.Popen(
                [exe, "--verify"],
                cwd=working_dir,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )
            stdout, stderr = proc.communicate(timeout=6)
            exit_code = proc.returncode
            duration = time.time() - start_time
            print(f"Mode: --verify | Exit code: {exit_code} | Duration: {duration:.2f}s")
            if stdout:
                print(f"  [STDOUT]: {stdout.strip()}")
            if stderr:
                print(f"  [STDERR]: {stderr.strip()}")
            results.append((name, "--verify", exit_code, stdout, stderr))
        except subprocess.TimeoutExpired:
            print(f"  [TIMEOUT]: Process timed out after 6s in --verify mode, terminating...")
            if proc:
                proc.kill()
                stdout, stderr = proc.communicate()
                print(f"  [STDOUT]: {stdout.strip() if stdout else ''}")
                print(f"  [STDERR]: {stderr.strip() if stderr else ''}")
            results.append((name, "--verify (timeout)", -999, stdout if proc else "", stderr if proc else ""))
        except Exception as e:
            print(f"  [EXCEPTION]: {e}")
            results.append((name, "--verify (exception)", -998, "", str(e)))
        
        # Test 2: window mode (no --verify, launch, wait 1.5s, then terminate)
        start_time = time.time()
        proc = None
        try:
            proc = subprocess.Popen(
                [exe],
                cwd=working_dir,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )
            time.sleep(1.5)
            # Check if process is still alive or already crashed/exited
            poll_ret = proc.poll()
            if poll_ret is not None:
                stdout, stderr = proc.communicate()
                print(f"Mode: windowed | Process exited early with code {poll_ret}")
                if stdout:
                    print(f"  [STDOUT]: {stdout.strip()}")
                if stderr:
                    print(f"  [STDERR]: {stderr.strip()}")
                results.append((name, "windowed (early exit)", poll_ret, stdout, stderr))
            else:
                # Terminate running window
                proc.terminate()
                try:
                    stdout, stderr = proc.communicate(timeout=2)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    stdout, stderr = proc.communicate()
                print(f"Mode: windowed | Window ran successfully without crash for 1.5s, closed cleanly.")
                results.append((name, "windowed (clean run)", 0, stdout, stderr))
        except Exception as e:
            print(f"  [EXCEPTION in windowed mode]: {e}")
            results.append((name, "windowed (exception)", -998, "", str(e)))
        
        print()

    print("\n" + "="*60)
    print("SUMMARY OF ALL SAMPLES TEST RESULTS")
    print("="*60)
    failures = []
    for name, mode, code, stdout, stderr in results:
        status = "PASS" if code == 0 else f"FAIL ({code})"
        if code != 0:
            failures.append((name, mode, code, stderr))
        print(f"[{status:12}] {name:30} | {mode}")
    
    print("\nTotal tests:", len(results))
    print(f"Total failures: {len(failures)}")
    if failures:
        print("\nFailed test details:")
        for name, mode, code, err in failures:
            print(f"  - {name} ({mode}): code={code}, error={err.strip()}")

if __name__ == "__main__":
    main()
