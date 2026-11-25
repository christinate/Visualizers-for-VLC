# Remote test-package cleanup

Completed 2026-11-17 on the owner's Mac and Mint machines. This removes old
expanded 0.4 test packages; it does not uninstall the owner's VLC or other apps.

| Machine | Expanded package copies removed | Free bytes reclaimed | Free bytes afterward |
| --- | ---: | ---: | ---: |
| macbook1 | 3 | 720,379,904 | 6,037,585,920 |
| mint | 3 | 1,105,403,904 | 1,834,782,720 |

The removed Mac directories were `package-macos-VLC-Visualizer-Addon-0.4.0-rc.1`,
`release-suite-macos` and `suite-0.4b`. The removed Mint directories were
`package-linux-VLC-Visualizer-Addon-0.4.0-rc.1`, `suite-0.4` and
`installer-0.4-test`, beneath each machine's `~/vlc-visualizers-work/`.

All four compressed 0.4 releases remain on each machine. Their hashes match
the protected release copies on the owner's workstation. Every regular file
in the removed directories was either byte-identical to a retained archive
member or saved in a verified backup. The Mac backup contains 16 unique file
contents (1,145,310 uncompressed bytes); Mint's contains 26 (1,263,126 bytes).
These include the unique reports and test settings. Link targets and file
metadata are retained in the cleanup manifest. The compressed unique backups
are 404,952 and 497,115 bytes, respectively.

The manifests and unique backups remain in each machine's
`~/vlc-visualizers-work/cleanup-audit-20261117-v1/`. Manifest copies are also
under the workstation's `build/remote-cleanup-20261117-v1/` directory. Sources,
current native builds, pinned dependencies and unique reference captures were
preserved. No process was using a selected directory when it was removed.

The free-space values are point-in-time filesystem measurements. On APFS,
reclaimed space can differ from the total logical size of removed files.
