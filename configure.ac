#                                               -*- Autoconf -*-

AC_PREREQ([2.60])
AC_INIT([knot], [1.5.0-alpha], [knot-dns@labs.nic.cz])
AM_INIT_AUTOMAKE([gnits subdir-objects dist-xz -Wall -Werror])
AM_SILENT_RULES([yes])
AC_CONFIG_SRCDIR([src/knot/main.c])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_USE_SYSTEM_EXTENSIONS([_GNU_SOURCE])

# Automatically update release date based on configure.ac date
release_date=`doc/mdate-sh configure.ac`
AC_SUBST([RELEASE_DATE], $release_date)

# Set compiler compatibility flags
AC_PROG_CC_C99
AM_PROG_CC_C_O
AC_PROG_CPP_WERROR

# Default compiler flags
CFLAGS="$CFLAGS -Wall -Werror=format-security"

# Checks for programs.
m4_ifdef([AM_PROG_AR], [AM_PROG_AR]) # call AM_PROG_AR only if available

# Initialize libtool
AC_DISABLE_STATIC
AC_PROG_LIBTOOL
LT_INIT

# Use pkg-config
PKG_PROG_PKG_CONFIG

AC_CACHE_CHECK([for reentrant lex], [ac_cv_path_LEX],
  [AC_PATH_PROGS_FEATURE_CHECK([LEX], [$LEX flex gflex],
  [cat >conftest.l <<_ACEOF
%{
%}

%option reentrant
%option bison-bridge
%option noinput
%option nounput
%option noreject

BLANK [ \t\n]

%%
<<EOF>> return 0;
%%
_ACEOF
_AC_DO_VAR(ac_path_LEX conftest.l)
test $ac_status -eq 0 && ac_cv_path_LEX=$ac_path_LEX ac_path_LEX_found=true
rm -f conftest.l lexyy.c lex.yy.c
],
[AC_MSG_ERROR([could not find lex that supports reentrant parsers])])])
AC_SUBST([LEX], [$ac_cv_path_LEX])
AM_PROG_LEX

AC_PROG_YACC
YACC_BISON=`bison --version | awk '{print $1;exit}'`
AS_IF([test "x$YACC_BISON" != "xbison"],
    [AC_MSG_ERROR([GNU bison needed for reentrant parsers, set the \$YACC variable before running configure])])
AC_PROG_INSTALL

# Check for Ragel
AC_ARG_WITH([ragel],
    AC_HELP_STRING([--with-ragel=path], [Path to Ragel binary. [default=auto]]),
    [with_ragel=$withval],
    [with_ragel=auto])

AS_CASE([$with_ragel],
    [yes|auto], [AC_PATH_PROG([RAGEL], [ragel], [false])],
    [no], [RAGEL=false],
    [*], [RAGEL=$with_ragel],
)

AM_CONDITIONAL([HAVE_RAGEL], test "$RAGEL" != "false")

# Set FSM type for Ragel
AC_ARG_ENABLE([fastparser],
    AS_HELP_STRING([--disable-fastparser], [Don't use faster zone parser]),
    [],
    [enable_fastparser=yes])

AS_CASE([$enable_fastparser],
    [no], [AC_SUBST([FSM_TYPE], [-T0])],
    [yes], [AC_SUBST([FSM_TYPE], [-G2])],
    [*], [
        AS_IF([test "$RAGEL" = "false"],
	    [AC_MSG_ERROR([Ragel is needed to generate different parsers])])
        AC_SUBST([FSM_TYPE], [$enable_fastparser])
    ])
AM_CONDITIONAL([G2_PARSER], test "$enable_fastparser" = "yes")
AM_CONDITIONAL([T0_PARSER], test "$enable_fastparser" = "no")

# Systemd integration
AC_ARG_ENABLE([systemd],
    AS_HELP_STRING([--enable-systemd=auto|yes|no], [enable systemd notification mechanism [default=auto]]),
    [enable_systemd="$enableval"], [enable_systemd=auto])

case "$enable_systemd" in
    auto) PKG_CHECK_MODULES([systemd_daemon], [libsystemd-daemon], [enable_systemd=yes], [enable_systemd=no]) ;;
    yes)  PKG_CHECK_MODULES([systemd_daemon], [libsystemd-daemon]) ;;
    no)   ;;
    *)    AC_MSG_ERROR([Invalid value of --enable-systemd.]) ;;
esac

if test "$enable_systemd" = yes; then
    AC_DEFINE([ENABLE_SYSTEMD_NOTIFY], [1], [Use systemd notifications.])
fi

# Debug modules
AC_ARG_ENABLE([debug],
    AS_HELP_STRING([--enable-debug=server,zones,xfr,packet,rr,ns,loader,dnssec],
    [compile selected debug modules [default=none]]),
    [
    echo ${enableval}|tr "," "\n"|while read val; do
        case "${val}" in
            server) AC_DEFINE([KNOTD_SERVER_DEBUG], [1], [Server debug.]) ;;
            zones) AC_DEFINE([KNOT_ZONES_DEBUG], [1], [Zones debug.]) ;;
            xfr) AC_DEFINE([KNOT_XFR_DEBUG], [1], [XFR debug.]) ;;
            packet) AC_DEFINE([KNOT_PACKET_DEBUG], [1], [Packet debug.]) ;;
            rr) AC_DEFINE([KNOT_RR_DEBUG], [1], [RR debug.]) ;;
            ns) AC_DEFINE([KNOT_NS_DEBUG], [1], [Nameserver debug.]) ;;
            loader) AC_DEFINE([KNOT_LOADER_DEBUG], [1], [Zone loading debug.]) ;;
            dnssec) AC_DEFINE([KNOT_DNSSEC_DEBUG], [1], [DNSSEC debug.]) ;;
        esac
    done
    ], [])

# Debug level
AC_ARG_ENABLE([debuglevel],
    AS_HELP_STRING([--enable-debuglevel=brief|verbose|details], [enable given debug level [default=disabled]]),
    # Not all shells support fall-through with ;& so I have to duplicate
    [case "x${enableval}" in
      xdetails)
        AC_DEFINE([DEBUG_ENABLE_DETAILS], [1], [Enable details debugging messages.])
        AC_DEFINE([DEBUG_ENABLE_VERBOSE], [1], [Enable verbose debugging messages.])
        AC_DEFINE([DEBUG_ENABLE_BRIEF], [1], [Enable brief debugging messages.])
        ;;
      xverbose)
        AC_DEFINE([DEBUG_ENABLE_VERBOSE], [1], [Enable verbose debugging messages.])
        AC_DEFINE([DEBUG_ENABLE_BRIEF], [1], [Enable brief debugging messages.])
        ;;
      xbrief)
        AC_DEFINE([DEBUG_ENABLE_BRIEF], [1], [Enable brief debugging messages.])
        ;;
    esac], [])

# recvmmsg() (valgrind doesn't support it, so disable for debugging)
# The check for struct mmsghdr is required when libc doesn't have an API but the function links
AC_ARG_ENABLE([recvmmsg],
    AS_HELP_STRING([--enable-recvmmsg=yes|no], [enable recvmmsg() network API under Linux (kernel support required) (set to 'no' if you have trouble running server under valgrind) [default=yes]]),
    [case "${enableval}" in
      yes)
	  AC_CHECK_FUNCS([sendmmsg])
          AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <sys/socket.h>]], [[struct mmsghdr v; recvmmsg(0,0,0,0,0);]])],
          [AC_DEFINE(HAVE_RECVMMSG, 1, [Define if struct mmsghdr and recvmmsg() exists.])])
          ;;
      no)
          ;;
      *)
          AC_MSG_ERROR([bad value ${enableval} for --enable-recvmmsg])
          ;;
    esac],
    [
      AC_CHECK_FUNCS([sendmmsg])
      AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <sys/socket.h>]], [[struct mmsghdr v; recvmmsg(0,0,0,0,0);]])],
      [AC_DEFINE(HAVE_RECVMMSG, 1, [Define if struct mmsghdr and recvmmsg() exists.])])
    ])

# Check for link time optimizations support and predictive commoning
AC_ARG_ENABLE([lto],
    AS_HELP_STRING([--enable-lto=yes|no], [enable link-time optimizations, enable if not broken for some extra speed [default=no]]),
    [case "${enableval}" in
      yes) AX_CHECK_COMPILE_FLAG("-flto", [CFLAGS="$CFLAGS -flto"], []) ;;
      no) ;;
      *)  AC_MSG_ERROR([bad value ${enableval} for --enable-lto]) ;;
    esac])

# Check for enable microseconds in log messages
AC_ARG_ENABLE([microseconds-log],
    AS_HELP_STRING([--enable-microseconds-log], [enable microseconds in log messages [default=no]]),
    AC_DEFINE([ENABLE_MICROSECONDS_LOG], [1], [microseconds in log messages]))

AX_CHECK_COMPILE_FLAG("-fpredictive-commoning", [CFLAGS="$CFLAGS -fpredictive-commoning"], [], "-Werror")

# Disable strict aliasing
# FIXME: we break strict aliasing on many occurenes, disable it until resolved or kept
AX_CHECK_COMPILE_FLAG("-fno-strict-aliasing", [CFLAGS="$CFLAGS -fno-strict-aliasing"], [])

# Default directories
run_dir="${localstatedir}/run/knot"
AC_ARG_WITH([rundir],
        AC_HELP_STRING([--with-rundir=path], [Path to run-time variable data (pid, sockets...). [default=LOCALSTATEDIR/run/knot]]),
        [run_dir=$withval])
AC_SUBST(run_dir)

storage_dir="${localstatedir}/lib/knot"
AC_ARG_WITH([storage],
        AC_HELP_STRING([--with-storage=path], [Default storage directory (slave zones, persistent data). [default=LOCALSTATEDIR/lib/knot]]),
        [storage_dir=$withval])
AC_SUBST(storage_dir)

config_dir="${sysconfdir}/knot"
AC_ARG_WITH([configdir],
        AC_HELP_STRING([--with-configdir=path], [Default directory for configuration. [default=SYSCONFDIR/knot]]),
        [config_dir=$withval])
AC_SUBST(config_dir)

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lm':

dnl Check for userspace-rcu library
AC_ARG_WITH(urcu,
[  --with-urcu=DIR        where to find userspace-rcu library])

AS_IF([test "$with_urcu" != "no"], [
  PKG_CHECK_MODULES([liburcu], liburcu, [
    CPPFLAGS="$CPPFLAGS $liburcu_CFLAGS"
    LIBS="$LIBS $liburcu_LIBS"
    with_urcu=yes
  ],[
    for try_urcu in "$with_urcu" "" "/usr/local"; do
      save_LIBS="$LIBS"
      save_CPPFLAGS="$CPPFLAGS"

      AS_IF([test -d "$try_urcu"], [
        liburcu_CFLAGS="-I$try_urcu/include"
        liburcu_LIBS="-L$try_urcu/lib"
      ],[
        liburcu_CFLAGS=""
        liburcu_LIBS=""
      ])

      CPPFLAGS="$CPPFLAGS $liburcu_CFLAGS"
      LIBS="$LIBS $liburcu_LIBS"

      AC_SEARCH_LIBS([rcu_set_pointer_sym], [urcu], [
        with_urcu=yes
	break
      ],[
        CPPFLAGS="$save_CPPFLAGS"
        LIBS="$save_LIBS"
        with_urcu=no
	# do not cache result of AC_SEARCH_LIBS test
	unset ac_cv_search_rcu_set_pointer_sym
      ])
    done

    AS_IF([test "$with_urcu" = "no"],[
      AC_MSG_ERROR([liburcu is required])
    ])
  ])
])

AC_SEARCH_LIBS([urcu_init], [urcu], [AC_MSG_ERROR([liburcu is too old (< 0.4.0), urcu_init symbol found])], [])

dnl Check for OpenSSL
AC_ARG_WITH(openssl,
[  --with-openssl=DIR        where to find openssl library])

AS_IF([test "$with_openssl" != "no"],[
  PKG_CHECK_MODULES([libcrypto], libcrypto, [
    CPPFLAGS="$CPPFLAGS $libcrypto_CFLAGS"
    LIBS="$LIBS $libcrypto_LIBS"
    with_openssl=yes
  ],[

    for try_openssl in "$with_openssl" "" "/usr/local"; do
      save_LIBS="$LIBS"
      save_CPPFLAGS="$CPPFLAGS"

      AS_IF([test -d "$try_openssl"], [
        libcrypto_CFLAGS="-I$try_openssl/include"
        libcrypto_LIBS="-L$try_openssl/lib"
      ],[
        libcrypto_CFLAGS=""
        libcrypto_LIBS=""
      ])

      CPPFLAGS="$CPPFLAGS $libcrypto_CFLAGS"
      LIBS="$LIBS $libcrypto_LIBS"

      AC_SEARCH_LIBS([OpenSSL_add_all_digests], [crypto], [
        with_openssl=yes
	break
      ],[
        with_openssl=no
	# do not cache result of AC_SEARCH_LIBS test
	unset ac_cv_search_OpenSSL_add_all_digests
      ])

      CPPFLAGS="$save_CPPFLAGS"
      LIBS="$save_LIBS"
    done

  ])
])

AS_IF([test "$with_openssl" = "no"],[
  AC_MSG_ERROR([OpenSSL library is required.])
])

AC_EGREP_CPP(openssl_version_ok,
  [#include <openssl/crypto.h>
   #if (OPENSSL_VERSION_NUMBER >= 0x10000000L)
   openssl_version_ok
   #endif
  ],[],[AC_MSG_ERROR([OpenSSL library version >= 1.0.0 is required.])]
)

AC_ARG_WITH(libidn, AC_HELP_STRING([--with-libidn=[DIR]],
    [Support IDN (needs GNU Libidn)]),
    libidn=$withval, libidn=yes)
if test "$libidn" != "no"; then
    if test "$libidn" != "yes"; then
        LDFLAGS="${LDFLAGS} -L$libidn/lib"
        CPPFLAGS="${CPPFLAGS} -I$libidn/include"
    fi
    AC_CHECK_HEADER(idna.h, AC_CHECK_LIB(idn, stringprep_check_version,
                                 [libidn=yes LIBS="${LIBS} -lidn"], libidn=no),
                    libidn=no)
fi
if test "$libidn" != "no" ; then
    AC_DEFINE(LIBIDN, 1, [Define to 1 if you want IDN support.])
fi

dnl Check for dnstap.
dt_DNSTAP([
    LIBS="$LIBS $DNSTAP_LIBS"
    CPPFLAGS="$CPPFLAGS $DNSTAP_CFLAGS"
    AC_DEFINE([USE_DNSTAP], [1], [Define to 1 to enable dnstap support])
])
AM_CONDITIONAL([HAVE_DNSTAP], test "$opt_dnstap" != "no")

AC_SEARCH_LIBS([pow], [m])
AC_SEARCH_LIBS([pthread_create], [pthread], [], [AC_MSG_ERROR([pthreads not found])])
AC_SEARCH_LIBS([dlopen], [dl])
AC_SEARCH_LIBS([clock_gettime], [rt])
AC_SEARCH_LIBS([capng_apply], [cap-ng])
AC_SEARCH_LIBS([adler32], [z])

# Checks for header files.
AC_HEADER_RESOLV
AC_CHECK_HEADERS_ONCE([cap-ng.h netinet/in_systm.h pthread_np.h signal.h sys/select.h sys/time.h sys/wait.h sys/uio.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([clock_gettime gettimeofday fgetln getline madvise malloc_trim poll posix_memalign pselect pthread_setaffinity_np regcomp select setgroups initgroups])

# Check for be64toh function
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <endian.h>]], [[return be64toh(0);]])],
[AC_DEFINE(HAVE_BE64TOH, 1, [Define to 1 if you have the 'be64toh' function.])])

# Check for cpu_set_t/cpuset_t compatibility
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <pthread.h>]], [[cpu_set_t set; CPU_ZERO(&set);]])],
[AC_DEFINE(HAVE_CPUSET_LINUX, 1, [Define if Linux-like cpu_set_t exists.])])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <pthread_np.h>]], [[cpuset_t set; CPU_ZERO(&set);]])],
[AC_DEFINE(HAVE_CPUSET_BSD, 1, [Define if FreeBSD-like cpuset_t exists.])])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <sched.h>]], [[cpuset_t* set = cpuset_create(); cpuset_destroy(set);]])],
[AC_DEFINE(HAVE_CPUSET_NETBSD, 1, [Define if cpuset_t and cpuset(3) exists.])])

# Use -fvisibility=hidden when linking.
# Let's leave it default now and use -export-symbols-regex in LDFLAGS
#gl_VISIBILITY()
#CFLAGS="$CFLAGS $CFLAG_VISIBILITY"

# Add code coverage macro
AX_CODE_COVERAGE

AC_CONFIG_FILES([Makefile
		 doc/Makefile
		 man/Makefile
		 samples/Makefile
		 patches/Makefile
		 libtap/Makefile
		 src/Makefile
		 tests/Makefile
		 src/dnstap/Makefile
		 src/zscanner/Makefile
		 man/khost.1
		 man/knotc.8
		 man/knotd.8
		 man/kdig.1
		 man/knsupdate.1
		 man/knot.conf.5
		 man/knsec3hash.1
		 ])

AC_OUTPUT

echo "
  Version: ${PACKAGE_VERSION}
  Prefix: ${prefix}
  Run dir: ${run_dir}
  Storage dir: ${storage_dir}
  Config dir: ${config_dir}
  Compiler: ${CC}
  CFlags: ${CFLAGS} ${CPPFLAGS}
  LDFlags: ${LDFLAGS}
  Libs: ${LIBS}
  Ragel: ${RAGEL} ${FSM_TYPE}
  Utils with IDN: ${libidn}
  Use systemd notifications: ${enable_systemd}
  Use dnstap support: ${opt_dnstap}
  Code coverage: ${enable_code_coverage}

  Continue with 'make' command"
