import subprocess

print("Initializing git and updating remote...")
subprocess.run(["git", "init"])
subprocess.run(["git", "branch", "-M", "main"])
subprocess.run(["git", "remote", "remove", "origin"], stderr=subprocess.DEVNULL)
subprocess.run(["git", "remote", "add", "origin", "https://github.com/jason8105/Subway_zygisk_touch_integration.git"])
subprocess.run(["git", "add", "."])
subprocess.run(["git", "commit", "-m", "feat: update subway zygisk touch integration"])
subprocess.run(["git", "push", "-u", "origin", "main", "--force"])

print("Git operations completed successfully!")
