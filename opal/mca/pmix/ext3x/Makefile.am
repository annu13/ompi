#
# Copyright (c) 2014-2016 Intel, Inc.  All rights reserved.
# Copyright (c) 2015      Cisco Systems, Inc.  All rights reserved.
# Copyright (c) 2015      Research Organization for Information Science
#                         and Technology (RIST). All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

sources = \
        pmix3x.h \
        pmix3x_component.c \
        pmix3x.c \
        pmix3x_client.c \
        pmix3x_server_south.c \
        pmix3x_server_north.c

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_opal_pmix_ext3x_DSO
component_noinst =
component_install = mca_pmix_ext3x.la
else
component_noinst = libmca_pmix_ext3x.la
component_install =
endif

mcacomponentdir = $(opallibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_pmix_ext3x_la_SOURCES = $(sources)
mca_pmix_ext3x_la_CFLAGS = $(opal_pmix_ext3x_CFLAGS)
mca_pmix_ext3x_la_CPPFLAGS =$(opal_pmix_ext3x_CPPFLAGS)
mca_pmix_ext3x_la_LDFLAGS = -module -avoid-version $(opal_pmix_ext3x_LDFLAGS)
mca_pmix_ext3x_la_LIBADD = $(opal_pmix_ext3x_LIBS)

noinst_LTLIBRARIES = $(component_noinst)
libmca_pmix_ext3x_la_SOURCES =$(sources)
libmca_pmix_ext3x_la_CFLAGS = $(opal_pmix_ext3x_CFLAGS)
libmca_pmix_ext3x_la_CPPFLAGS = $(opal_pmix_ext3x_CPPFLAGS)
libmca_pmix_ext3x_la_LDFLAGS = -module -avoid-version $(opal_pmix_ext3x_LDFLAGS)
libmca_pmix_ext3x_la_LIBADD = $(opal_pmix_ext3x_LIBS)
