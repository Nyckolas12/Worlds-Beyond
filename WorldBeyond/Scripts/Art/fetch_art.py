"""
Downloads generated UI art (Plan 6) listed in Art/UI/Source/downloads.tsv: "<path under Art/UI/Source>\t<url>" per line.
Files already on disk are skipped. Run with the system Python: python Scripts/Art/fetch_art.py
"""
import os
import subprocess
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Art", "UI", "Source")
LIST = os.path.join(ROOT, "downloads.tsv")


def main():
    fetched = 0
    for line in open(LIST, encoding="utf-8"):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        rel, url = line.split("\t")
        path = os.path.join(ROOT, rel)
        if os.path.exists(path) and os.path.getsize(path) > 0:
            continue
        os.makedirs(os.path.dirname(path), exist_ok=True)
        # curl: the CDN refuses Python's default user agent
        result = subprocess.run(["curl", "-s", "-f", "--max-time", "120", "-o", path, url])
        print(("ok   " if result.returncode == 0 else "FAIL ") + rel)
        fetched += result.returncode == 0
    print("%d fetched" % fetched)


if __name__ == "__main__":
    sys.exit(main())
