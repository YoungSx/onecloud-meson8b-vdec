# Contributing

Use Conventional Commits: `feat(vdec): ...`, `fix(runtime): ...`,
`docs: ...`, `test: ...`, `build: ...`, `ci: ...`. Keep hardware changes,
build changes and documentation reviewable as separate commits.

Run `make test` before submitting. It fetches pinned firmware sources,
runs Python tests, C ASan/UBSan checks and documentation links. Run
`bash scripts/build-kernel.sh` for driver changes, and regenerate the
additive patch with `scripts/export-kernel-patch.py` on the clean matching
baseline when codec sources change. CI rejects stale patch content.

[Build instructions](docs/build.md) list cross-toolchain dependencies.
[CI and release instructions](docs/release.md) describe required jobs and
artifact provenance. Keep the English and Chinese README sections equivalent.
Documentation links should be relative so GitHub and release source archives
can resolve them.

Driver changes require a separate hardware report: kernel, source commit,
module hashes, stream metadata, complete frame count/content, error recovery,
services and rollback. Record failures and limits; successful build/exit codes
are not image correctness. Existing dated evidence is a snapshot, not a
current claim. Do not change `/boot` or assume another board's phandles.
