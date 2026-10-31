# Original verification files: migration evidence

The 10 `.pml` models and original `README.md` were copied **byte-for-byte**
from `qigao/salts@codex/finish-1181-session/utils/verification` Git tree
`9c5b53d7194d142a89be576c3314ee4bfd569a37`.

Do not "clean up" or delete the original models when CMeta is separated:
models describe owner, claim/publish/release, memory buffers, SPSC, priority
queues and Disruptor topology. They are valuable original acceptance inputs
even when the C11 implementation has moved to independent Core/Concurrency
modules.

`tools/check_original_promela_sources.py` verifies all 11 original blobs in
Core SDK CI; `verification/CMakeLists.txt` lists the source assets and fails
if any are absent. **Source preservation is not model verification**.
The original README documents 10 models, 50 claims and Win32 Spin 6.5.1
procedures. Those claims have not been rerun on this PR and cannot be reported
as accepted or used to close Issue #71. Additional tests, ASan/UBSan/TSan and
original behavior comparison remain required.
