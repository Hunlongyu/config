"""Validate local Markdown links and snippets copied from compiled examples."""
from pathlib import Path
import argparse
import re
import sys

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("--update", action="store_true")
args = parser.parse_args()
documents = [root / name for name in ("README.md", "README.zh-CN.md", "CONTRIBUTING.md")]
documents += sorted((root / "docs").glob("*.md"))
pattern = re.compile(r"<!-- snippet:([^:]+):([\w-]+) -->\s*(.*?)<!-- /snippet -->", re.S)
errors = []
for document in documents:
    text = document.read_text(encoding="utf-8-sig")
    def snippet(match):
        source, name = match.group(1, 2)
        code = (root / source).read_text(encoding="utf-8-sig")
        begin = f"// snippet:begin {name}\n"
        end = f"// snippet:end {name}"
        if begin not in code or end not in code:
            errors.append(f"{source}: missing snippet {name}")
            return match.group(0)
        code = code.split(begin, 1)[1].split(end, 1)[0].strip()
        fence = chr(96) * 3
        expected = f"<!-- snippet:{source}:{name} -->\n{fence}cpp\n{code}\n{fence}\n<!-- /snippet -->"
        if not args.update and match.group(0) != expected:
            errors.append(f"{document.relative_to(root)}: stale snippet {name}; run --update")
        return expected if args.update else match.group(0)
    updated = pattern.sub(snippet, text)
    if args.update and updated != text:
        document.write_text(updated, encoding="utf-8")
    for target in re.findall(r"\[[^\]]*\]\(([^)]+)\)", updated):
        if re.match(r"[a-zA-Z]+:", target) or target.startswith("#"):
            continue
        target = target.split("#", 1)[0]
        if target and not (document.parent / target).exists():
            errors.append(f"{document.relative_to(root)}: missing local link {target}")
if errors:
    print("\n".join(errors), file=sys.stderr)
    sys.exit(1)
print(f"Validated {len(documents)} documents and their compiled-example snippets.")
