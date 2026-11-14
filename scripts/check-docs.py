#!/usr/bin/env python3
"""Check local Markdown links in documentation and dated evidence."""
import re
from pathlib import Path
from urllib.parse import unquote, urlsplit


def check(root):
    documents = [*root.glob('*.md'), *root.glob('docs/**/*.md'),
                 *root.glob('evidence/**/*.md'), *root.glob('patches/**/*.md')]
    failures = []
    for path in documents:
        source = re.sub(r'```.*?```', '', path.read_text(), flags=re.S)
        for link in re.findall(r'!?\[[^\]]*\]\(([^)]+)\)', source):
            target = link.split(' "', 1)[0].strip('<>')
            url = urlsplit(target)
            if url.scheme or url.netloc or not url.path:
                continue
            if not (path.parent / unquote(url.path)).exists():
                failures.append(f'{path.relative_to(root)}: {target}')
    if failures:
        raise ValueError('Broken documentation links:\n' + '\n'.join(failures))
    return len(documents)


if __name__ == '__main__':
    print(f'PASS: local links in {check(Path(__file__).resolve().parent.parent)} Markdown files')
