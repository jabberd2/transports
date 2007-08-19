AC_INIT([tt],[0.3.6cvs],[mailto:movax@mpi.int.pl jid:movax@jabber.mpi.int.pl])
AM_INIT_AUTOMAKE([dist-bzip2])
AC_PROG_CC
AC_PROG_LIBTOOL

# small fix for alpha arch.  
if test $host_cpu = alpha; then 
   CFLAGS=`echo $CFLAGS | sed 's/-O2/-O/'`
   CXXFLAGS=`echo $CXXFLAGS | sed 's/-O2/-O/'`
fi

AC_ARG_ENABLE([dmalloc], AS_HELP_STRING([--enable-dmalloc],[link with dmalloc library (default=no)]))
if test x$enable_dmalloc = xyes ; then 
	AC_CHECK_LIB([dmalloc],[malloc],,[AC_MSG_ERROR([dmalloc not found - aborting...])])
fi

AC_ARG_ENABLE([gnugetopt], AS_HELP_STRING([--enable-gnugetopt],[link with gnugetopt library  (usefull for non-Linux systems) (default=no)]))
if test x$enable_gnugetopt = xyes ; then 
	AC_CHECK_LIB([gnugetopt],[getopt_long],,[AC_MSG_ERROR([gnugetopt not found - aborting...])],[-L/usr/local/lib])
fi

AC_ARG_ENABLE([debugsym], AS_HELP_STRING([--disable-debugsym]],[remove debuging symbols for gdb (default=no)]), [
case "${debugsymval}" in
  yes) debugsym=true ;;
  no) debugsym=false ;;
  "") debugsym=true ;;
  *) AC_MSG_ERROR(bad value ${debugsymval} for --enable-debugsym) ;;
esac
], [debugsym=true])
AM_CONDITIONAL(DEBUGSYM, test x$debugsym = xtrue)

AC_CHECK_LIB([xode],[xode_get_name],,[AC_MSG_ERROR([libxode not found - aborting...])])
AC_CHECK_LIB([crypto],[X509_CRL_add0_revoked],,[AC_MSG_ERROR([libcrypto not found - aborting...])])
AC_CHECK_LIB([ssl],[EVP_MD_CTX_init],,[AC_MSG_ERROR([libssl not found - aborting...])],[-lcrypto])
AC_CHECK_LIB([tlen],[tlen_getroster],,[AC_MSG_ERROR([libtlen not found - aboring...])])

AC_CONFIG_FILES(Makefile
		src/Makefile
		src/jabber/Makefile)
AC_OUTPUT
AC_DEFINE_UNQUOTED(VERSION, "$VERSION", [Version number of package.])
