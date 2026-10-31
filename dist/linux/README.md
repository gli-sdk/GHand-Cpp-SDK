# Linux release

For example 15, complete the one-time permission setup below, then run `./15_tactile_display`.
Copy this entire directory; the entry scripts locate bundled libraries and config.
Built on Linux x86_64, glibc glibc 2.35.
Use a compatible Linux system with a graphical session for example 15.
glibc, the ELF loader, graphics/device drivers and fonts remain system dependencies.
EtherCAT requires raw socket privileges and a connected hand.
Once after deployment: `python3 setup_ethercat.py` (requires patchelf and setcap).
The setup patches the actual ELF with an absolute DT_RPATH, then invokes sudo
only for setcap cap_net_raw=ep. Run `./15_tactile_display` as your normal user.
For another example: `python3 setup_ethercat.py 01_basic_connection`.
Moving or replacing the bundle requires running setup again. Setup refreshes checksums.
Use --prepare-only to prepare paths and print the authorization command.
Qt and other bundled shared libraries can be replaced in lib/ and plugins/.
Package/source versions are in PACKAGES.tsv; Debian/Ubuntu copyright notices
and source locations are in LICENSES/packages/. Obtain matching distribution
sources with `apt-get source <source>=<source_version>` using deb-src repositories.
