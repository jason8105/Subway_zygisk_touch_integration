import os
import datetime
import subprocess

def run_git_push():
    timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    commit_msg = f"Auto update from local device: {timestamp}"
    
    print("[*] Adding all changes...")
    subprocess.run(["git", "add", "."], capture_output=True, text=True)
    
    print(f"[*] Committing with message: '{commit_msg}'")
    subprocess.run(["git", "commit", "-m", commit_msg], capture_output=True, text=True)
    
    print("[*] Pushing to GitHub (main branch)...")
    result = subprocess.run(["git", "push", "origin", "main", "--force"], capture_output=True, text=True)
    
    if result.returncode == 0:
        print("==================================================")
        print(f" SUCCESS! Code pushed successfully at {timestamp}")
        print("==================================================")
    else:
        print("[!] Push failed or nothing to commit:")
        print(result.stdout + result.stderr)

if __name__ == "__main__":
    run_git_push()
