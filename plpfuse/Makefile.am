# plpfuse/Makefile.am
#
# This file is part of plptools.
#
# Copyright (C) 2007-2013 Reuben Thomas <rrt@sc3d.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# along with this program; if not, see <https://www.gnu.org/licenses/>.

AM_CPPFLAGS=-I$(top_srcdir)/lib $(FUSE_CFLAGS)

sbin_PROGRAMS = plpfuse
plpfuse_LDADD = $(LIB_PLP) $(INTLLIBS) $(FUSE_LIBS)
plpfuse_SOURCES = main.cc fuse.c

EXTRA_DIST = rfsv_api.h plpfuse.h
