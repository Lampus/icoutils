ACLOCAL_AMFLAGS = -I m4

SUBDIRS = po lib common icotool wrestool extresso

.PHONY: rpm

EXTRA_DIST = \
  data/icons/icon-linux_penguin-20x20-16c.png \
  data/icons/icon-linux_penguin-16x16-16c.png \
  data/icons/icon-debian_old_bird-20x20-16c.png \
  data/resscripts/example \
  data/resscripts/win98_dlls \
  data/resscripts/win98_dlls_unfinished \
  data/resscripts/win98_executables \
  data/resscripts/win98_moricons \
  data/resscripts/win98_pifmgr \
  data/resscripts/win98_shell32 \
  @PACKAGE@.spec.in \
  MANIFEST.sources

rpm: @PACKAGE@.spec
	fakeroot rpmbuild --clean -bb @PACKAGE@.spec

distclean-local:
	-rm -f @PACKAGE@.spec
