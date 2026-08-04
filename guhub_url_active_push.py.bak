import subprocess

commands = [
    ["git", "init"],
    ["git", "branch", "-M", "main"],
    ["git", "remote", "add", "origin", "https://github.com/jason8105/Subway_zygisk_touch_integration.git"],
    ["git", "add", "."],
    ["git", "commit", "-m", "chore: initialize git repository and clean structure for zygisk"],
    ["git", "push", "-u", "origin", "main", "--force"]
]

for cmd in commands:
    print(f"Executing: {' '.join(cmd)}")
    subprocess.run(cmd)

print("Git operations completed!")
