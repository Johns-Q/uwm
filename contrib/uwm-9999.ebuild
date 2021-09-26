# Copyright 1999-2011 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI="7"

inherit eutils
inherit toolchain-funcs

if [[ ${PV} == "9999" ]] ; then
	inherit git-r3
	EGIT_REPO_URI="https://git.code.sf.net/p/${PN}/code"
	EGIT_OVERRIDE_REPO_CORE_ARRAY_CODE="https://git.code.sf.net/p/core-array/code"
	EGIT_OVERRIDE_REPO_CORE_RC_CODE="https://git.code.sf.net/p/core-rc/code"
else
	SRC_URI="mirror://sourceforge/${PN}/${P}.tar.bz2"
fi

DESCRIPTION="uwm - µ Window Manager"
HOMEPAGE="http://uwm.sourceforge.net"
LICENSE="AGPL-3"
SLOT="0"
KEYWORDS="~x86 ~amd64 ~arm"
IUSE="png jpeg xinerama"

DEPEND=">=x11-libs/libxcb-1.7
	x11-libs/xcb-util
	x11-libs/xcb-util-wm
	x11-libs/xcb-util-image
	x11-libs/xcb-util-keysyms
	x11-libs/xcb-util-renderutil
	x11-libs/libX11
	sys-devel/make
	media-fonts/font-alias
	media-fonts/font-misc-misc
	media-fonts/font-cursor-misc
	png? ( media-libs/libpng )
	jpeg? ( virtual/jpeg )
"
# xinerama? ( x11-proto/xineramaproto )

src_compile() {
	local myconf

	myconf="'-DSYSTEM_CONFIG=\"/etc/system.uwmrc\"' -DUSE_CORE_RC_GET_STRINGS"
	use png && myconf="${myconf} -DUSE_PNG"
	use jpeg && myconf="${myconf} -DUSE_JPEG"
	use xinerama && myconf="${myconf} -DUSE_XINERAMA"

	emake -j1 all contrib/uwm-helper CC="$(tc-getCC)" CFLAGS="${CFLAGS}" \
		LDFLAGS="${LDFLAGS}" CONFIG="${myconf}"
}

src_install() {
	echo "#!/bin/sh" > ${T}/uwm
	echo "exec /usr/bin/uwm" >> ${T}/uwm

	exeinto /etc/X11/Sessions
	doexe "${T}"/uwm

	dobin uwm contrib/uwm-helper
	doman uwm.1 uwmrc.5
	dodoc ChangeLog README.txt contrib/uwmrc.example
	doicon contrib/uwm16x16.xpm contrib/x.xpm
}

pkg_postinst() {
	ewarn "Please extract uwmrc.example from /usr/share/doc/${PF} to"
	ewarn "/etc/system.uwmrc or to your \$HOME/.uwm/uwmrc or/and edit it."
}
