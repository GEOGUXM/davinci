bin_PROGRAMS = davinci

if MODULE_SUPPORT
  noinst_LIBRARIES = libmodsupp.a
  libmodsupp_a_SOURCES = ff_modules.c module_io.c io_loadmod.c
  MY_MODULE_LDADD = libmodsupp.a
  MY_MODULES_SUBDIR = modules
endif

if LOCAL_READLINE
  MY_READLINE_LDADD = readline/libreadline.a
  MY_READLINE_SUBDIR = readline
endif

if LOCAL_IOMEDLEY
  MY_IOMEDLEY_LDADD = iomedley/libiomedley.a
  MY_IOMEDLEY_SUBDIR = iomedley
endif

if LOCAL_VICAR
  MY_VICAR_SUBDIR = vicar
endif

SUBDIRS = $(MY_READLINE_SUBDIR) $(MY_IOMEDLEY_SUBDIR) $(MY_VICAR_SUBDIR) $(MY_MODULES_SUBDIR) libltdl

davinci_LDADD = $(MY_READLINE_LDADD) $(MY_IOMEDLEY_LDADD) $(MY_MODULE_LDADD)
davinci_LDFLAGS = -export-dynamic

davinci_SOURCES = apifunc.c array.c avl.c binDCT.c \
	darray.c \
	dct.c deghost.C dvio.c dvio_ascii.c \
	dvio_aviris.c dvio_envi.c dvio_ers.c dvio_goes.c dvio_grd.c \
	dvio_hdf.c dvio_imath.c dvio_iomedley.c dvio_isis.c dvio_magic.c \
	dvio_pds.c dvio_pnm.c dvio_raw.c dvio_specpr.c \
	dvio_themis.c dvio_vicar.c endian_norm.c \
	error.c ff.c ff_ascii.c ff_avg.c \
	ff_bbr.c ff_cluster.c ff_convolve.c ff_deghost.c ff_display.c \
	ff_fft.c ff_filetype.c ff_fncc.c ff_gnoise.c ff_gplot.c \
	ff_header.c ff_hstretch.c ff_ifill.c ff_interp.c ff_ix.c \
	ff_load.c ff_loadvan.c ff_meta.c \
	ff_moment.c ff_pause.c ff_pbm.c ff_pca.c ff_plplot.c \
	ff_projection.c ff_random.c ff_raw.c ff_rgb.c ff_rice.c \
	ff_shade.c ff_slant.c ff_sort.c ff_source.c ff_struct.c \
	ff_text.c ff_version.c ff_vignette.c ff_write.c ff_xfrm.c \
	fft.c fft2f.c fft_mayer.c fit.c \
	header.c \
	help.c init.c \
	isis_obj.c main.c matrix.c \
	misc.c motif_tools.c \
	newfunc.c p.c pp.c \
	pp_math.c printf.c reserved.c \
	rfunc.c rice.c rpos.c scope.c \
	spread.c string.c symbol.c \
	system.c tools.c \
	ufunc.c util.c \
	x.c xrt_print_3d.c \
	lexer.c parser.c