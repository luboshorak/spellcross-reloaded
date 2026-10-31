# Original demo test branch

Build the application normally in Visual Studio. The post-build step assembles
`data/demo.dat` beside the executable from the two versioned binary parts and
checks its SHA-256. The split is only for repository transfer; the runtime file
is the unchanged original ZIP including README. Build-CombinedRelease.ps1 also
assembles this file into the release staging directory.

On first launch choose **Play the bundled original Spellcross demo** in the
source chooser. For an existing installation use the existing source-switch
option. New Game opens the original LEVEL_02.DEF scenario with two missions.
The profile caches and saves under demo_profile; normal retail paths remain
unchanged. The demo's original TUT04 references absent ELF1 sprites and reports
an explicit loading error. Its original data is not replaced with retail assets.

The demo-package workflow checks assembly on Windows and the production ZIP
importer plus production LevelLoader on Linux. The full-engine harness
`test_demo_data_load.cpp` additionally covers maps and the expected TUT04 error,
but has not been built or run in this environment. Full Windows application
build, interactive gameplay, sound and save/load still require manual testing.

The included original README documents installation and known problems; no
explicit redistribution grant was found in it. Original demo assets are not
covered by this repository's source-code license.
