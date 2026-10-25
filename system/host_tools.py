# input: E2FS_DIR or host PATH; output: e2fsprogs executable directory; pos: shared image-tool discovery.
from pathlib import Path
import os
import shutil

E2FS = Path(os.environ.get('E2FS_DIR', str(Path(
    shutil.which('debugfs') or '/opt/homebrew/opt/e2fsprogs/sbin/debugfs').parent)))
