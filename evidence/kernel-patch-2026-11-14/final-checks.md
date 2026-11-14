# Final additive patch verification

The first independent Kbuild compile exposed a pre-existing ARM printf
warning in esparser padding. Inspecting the path also found padding wrote
before capacity validation. The final patch validates the complete 4 KiB
minimum packet plus 512-byte search pattern before any write; failure
removes the timestamp, returns an error buffer and never starts parser DMA.

Host ASan/UBSan test checks too-small, exact-size, full-payload and UINT32_MAX
boundaries, proves failed writes leave the buffer unchanged, and checks the
search pattern and guard bytes. The final exported source builds without
compiler warnings (final-build.log). The final patch applies --fuzz=0 to
the matching unchanged Kconfig/Makefile fixture (final-apply-check.log).
The earlier full application and Kconfig module selection evidence is kept.

The rebuilt parser was loaded on the production target and decoded all
60 1080p Baseline frames; every NV12 frame MD5 matches the software baseline.
The codec/runtime modules were removed after the test; services stayed
active. Raw output: final-decoder.log and final-decoder.md5.
