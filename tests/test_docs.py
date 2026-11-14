import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    'check_docs', Path(__file__).resolve().parent.parent / 'scripts/check-docs.py')
docs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(docs)


class DocumentationTests(unittest.TestCase):
    def test_relative_links_and_missing_target(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'docs').mkdir()
            (root / 'docs/with space.md').write_text('content')
            readme = root / 'README.md'
            readme.write_text('[local](docs/with%20space.md) [web](https://example.com)')
            self.assertEqual(docs.check(root), 2)
            readme.write_text('[missing](docs/missing.md)')
            with self.assertRaisesRegex(ValueError, 'docs/missing.md'):
                docs.check(root)
