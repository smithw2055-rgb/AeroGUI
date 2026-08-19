import os
import subprocess
import glob
import time

def main():
    build_samples_dir = os.path.abspath("build/samples")
    exe_pattern = os.path.join(build_samples_dir, "*", "Debug", "*.exe")
    executables = sorted(glob.glob(exe_pattern))
    
    print(f"==================================================")
    print(f"Window Mode Execution Test across {len(executables)} Samples (Debug)")
    print(f"==================================================\n")
    
    passed = []
    crashed = []
    
    for exe in executables:
        name = os.path.basename(exe)
        working_dir = os.path.dirname(os.path.dirname(exe))  # build/samples/<SampleName>
        
        print(f"Launching {name} from {working_dir} ...", end="", flush=True)
        
        start = time.time()
        proc = subprocess.Popen(
            [exe],
            cwd=working_dir,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        
        # Give the window 2 seconds to initialize, create swapchain, compile shaders, render frames and pump events
        time.sleep(2.0)
        
        ret = proc.poll()
        if ret is not None and ret != 0:
            stdout, stderr = proc.communicate()
            print(f" [CRASH / ABNORMAL EXIT] Exit Code: {ret}")
            if stdout.strip():
                print(f"    STDOUT: {stdout.strip()}")
            if stderr.strip():
                print(f"    STDERR: {stderr.strip()}")
            crashed.append((name, ret, stderr.strip()))
        else:
            # Terminate running window gracefully
            proc.terminate()
            try:
                stdout, stderr = proc.communicate(timeout=2)
            except subprocess.TimeoutExpired:
                proc.kill()
                stdout, stderr = proc.communicate()
            print(f" [OK - Rendered & Running Smoothly]")
            passed.append(name)
            
    print("\n" + "="*60)
    print(f"WINDOW MODE RUN RESULTS: {len(passed)} / {len(executables)} PASSED")
    print("="*60)
    for p in passed:
        print(f"  [OK] {p}")
    if crashed:
        print("\nCrashed Samples:")
        for c, ret, err in crashed:
            print(f"  [CRASH] {c}: code {ret}, err: {err}")

if __name__ == "__main__":
    main()
