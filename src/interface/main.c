/*****************************************************************************
 * main.c: main vlc source
 * Includes the main() function for vlc. Parses command line, start interface
 * and spawn threads.
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: main.c,v 1.195.2.5 2002/07/29 16:12:24 gbazin Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <signal.h>                               /* SIGHUP, SIGINT, SIGKILL */
#include <stdio.h>                                              /* sprintf() */
#include <setjmp.h>                                       /* longjmp, setjmp */

#include <videolan/vlc.h>

#ifdef HAVE_GETOPT_LONG
#   ifdef HAVE_GETOPT_H
#       include <getopt.h>                                       /* getopt() */
#   endif
#else
#   include "GNUgetopt/getopt.h"
#endif

#ifdef SYS_DARWIN
#   include <mach/mach.h>                               /* Altivec detection */
#   include <mach/mach_error.h>       /* some day the header files||compiler *
                                                       will define it for us */
#   include <mach/bootstrap.h>
#endif

#ifndef WIN32
#   include <netinet/in.h>                            /* BSD: struct in_addr */
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( _MSC_VER ) && defined( _WIN32 )
#   include <io.h>
#endif

#ifdef HAVE_LOCALE_H
#   include <locale.h>
#endif

#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                  /* getenv(), strtol(),  */
#include <string.h>                                            /* strerror() */

#include "netutils.h"                                 /* network_ChannelJoin */

#include "stream_control.h"
#include "input_ext-intf.h"

#include "intf_playlist.h"
#include "interface.h"

#include "audio_output.h"

#include "video.h"
#include "video_output.h"

/*****************************************************************************
 * Configuration options for the main program. Each module will also separatly
 * define its own configuration options.
 * Look into configuration.h if you need to know more about the following
 * macros.
 *
 *****************************************************************************/
#define __BUILTIN__
#define MODULE_NAME main
#include "modules_inner.h"                        /* for configuration stuff */


#define INTF_TEXT N_("interface module")
#define INTF_LONGTEXT N_( \
    "This option allows you to select the interface used by vlc.\nNote that " \
    "the default behavior is to automatically select the best module " \
    "available.")

#define WARNING_TEXT N_("warning level (or use -v, -vv, etc...)")
#define WARNING_LONGTEXT N_( \
    "Increasing the warning level will allow you to see more debug messages " \
    "and can sometimes help you to troubleshoot a problem.")

#define STATS_TEXT N_("output statistics")
#define STATS_LONGTEXT N_( \
    "Enabling the stats mode will flood your log console with various " \
    "statistics messages.")

#define INTF_PATH_TEXT N_("interface default search path")
#define INTF_PATH_LONGTEXT N_( \
    "This option allows you to set the default path that the interface will " \
    "open when looking for a file.")

#define AOUT_TEXT N_("audio output module")
#define AOUT_LONGTEXT N_( \
    "This option allows you to select the audio audio output method used by " \
    "vlc.\nNote that the default behavior is to automatically select the " \
    "best method available.")

#define AUDIO_TEXT N_("enable audio")
#define AUDIO_LONGTEXT N_( \
    "You can completely disable the audio output. In this case the audio " \
    "decoding stage won't be done, and it will also save some " \
    "processing power.")

#define MONO_TEXT N_("force mono audio")
#define MONO_LONGTEXT N_("This will force a mono audio output")

#define VOLUME_TEXT N_("audio output volume")
#define VOLUME_LONGTEXT N_( \
    "You can set the default audio output volume here, in a range from 0 to " \
    "1024.")

#define FORMAT_TEXT N_("audio output format")
#define FORMAT_LONGTEXT N_( \
    "You can force the audio output format here.\n" \
    "0 -> 16 bits signed native endian (default)\n" \
    "1 ->  8 bits unsigned\n"                       \
    "2 -> 16 bits signed little endian\n"           \
    "3 -> 16 bits signed big endian\n"              \
    "4 ->  8 bits signed\n"                         \
    "5 -> 16 bits unsigned little endian\n"         \
    "6 -> 16 bits unsigned big endian\n"            \
    "7 -> mpeg2 audio (unsupported)\n"              \
    "8 -> ac3 pass-through")

#define RATE_TEXT N_("audio output frequency (Hz)")
#define RATE_LONGTEXT N_( \
    "You can force the audio output frequency here.\nCommon values are " \
    "48000, 44100, 32000, 22050, 16000, 11025, 8000.")

#define DESYNC_TEXT N_("compensate desynchronization of audio (in ms)")
#define DESYNC_LONGTEXT N_( \
    "This option allows you to delay the audio output. This can be handy if " \
    "you notice a lag between the video and the audio.")

#define VOUT_TEXT N_("video output module")
#define VOUT_LONGTEXT N_( \
    "This option allows you to select the video output method used by vlc.\n" \
    "Note that the default behavior is to automatically select the best " \
    "method available.")

#define VIDEO_TEXT N_("enable video")
#define VIDEO_LONGTEXT N_( \
    "You can completely disable the video output. In this case the video " \
    "decoding stage won't be done, and it will also save some " \
    "processing power.")

#define DISPLAY_TEXT N_("display identifier")
#define DISPLAY_LONGTEXT N_( \
    "This is the local display port that will be used for X11 drawing. " \
    "For instance :0.1.")

#define WIDTH_TEXT N_("video width")
#define WIDTH_LONGTEXT N_( \
    "You can enforce the video width here.\nNote that by default vlc will " \
    "adapt to the video characteristics.")

#define HEIGHT_TEXT N_("video height")
#define HEIGHT_LONGTEXT N_( \
    "You can enforce the video height here.\nNote that by default vlc will " \
    "adapt to the video characteristics.")

#define ZOOM_TEXT N_("zoom video")
#define ZOOM_LONGTEXT N_( \
    "You can zoom the video by the specified factor.")

#define GRAYSCALE_TEXT N_("grayscale video output")
#define GRAYSCALE_LONGTEXT N_( \
    "When enabled, the color information from the video won't be decoded " \
    "(this can also allow you to save some processing power).")

#define FULLSCREEN_TEXT N_("fullscreen video output")
#define FULLSCREEN_LONGTEXT N_( \
    "If this option is enabled, vlc will always start a video in fullscreen " \
    "mode.")

#define OVERLAY_TEXT N_("overlay video output")
#define OVERLAY_LONGTEXT N_( \
    "If enabled, vlc will try to take advantage of the overlay capabilities " \
    "of you graphics card.")

#define SPUMARGIN_TEXT N_("force SPU position")
#define SPUMARGIN_LONGTEXT N_( \
    "You can use this option to place the sub-titles under the movie, " \
    "instead of over the movie. Try several positions.")

#define FILTER_TEXT N_("video filter module")
#define FILTER_LONGTEXT N_( \
    "This will allow you to add a post-processing filter to enhance the " \
    "picture quality, for instance deinterlacing, or to clone or distort " \
    "the video window.")

#define SERVER_PORT_TEXT N_("server port")
#define SERVER_PORT_LONGTEXT N_( \
    "This is the port used for UDP streams. By default, we chose 1234.")

#define MTU_TEXT N_("MTU of the interface")
#define MTU_LONGTEXT N_( \
    "This is the typical size of UDP packets that we expect. On Ethernet " \
    "it is usually 1500.")

#define NETCHANNEL_TEXT N_("enable network channel mode")
#define NETCHANNEL_LONGTEXT N_( \
    "Activate this option if you want to use the VideoLAN Channel Server.")

#define CHAN_SERV_TEXT N_("channel server address")
#define CHAN_SERV_LONGTEXT N_( \
    "Indicate here the address of the VideoLAN Channel Server.")

#define CHAN_PORT_TEXT N_("channel server port")
#define CHAN_PORT_LONGTEXT N_( \
    "Indicate here the port on which the VideoLAN Channel Server runs.")

#define IFACE_TEXT N_("network interface")
#define IFACE_LONGTEXT N_( \
    "If you have several interfaces on your Linux machine and use the " \
    "VLAN solution, you may indicate here which interface to use.")

#define INPUT_PROGRAM_TEXT N_("choose program (SID)")
#define INPUT_PROGRAM_LONGTEXT N_( \
    "Choose the program to select by giving its Service ID.")

#define INPUT_AUDIO_TEXT N_("choose audio")
#define INPUT_AUDIO_LONGTEXT N_( \
    "Give the default type of audio you want to use in a DVD.")

#define INPUT_CHAN_TEXT N_("choose channel")
#define INPUT_CHAN_LONGTEXT N_( \
    "Give the stream number of the audio channel you want to use in a DVD " \
    "(from 1 to n).")

#define INPUT_SUBT_TEXT N_("choose subtitles")
#define INPUT_SUBT_LONGTEXT N_( \
    "Give the stream number of the subtitle channel you want to use in a DVD "\
    "(from 1 to n).")

#define DVD_DEV_TEXT N_("DVD device")
#define DVD_DEV_LONGTEXT N_( \
    "This is the default DVD device to use.")

#define VCD_DEV_TEXT N_("VCD device")
#define VCD_DEV_LONGTEXT N_( \
    "This is the default VCD device to use.")

#define IPV6_TEXT N_("force IPv6")
#define IPV6_LONGTEXT N_( \
    "If you check this box, IPv6 will be used by default for all UDP and " \
    "HTTP connections.")

#define IPV4_TEXT N_("force IPv4")
#define IPV4_LONGTEXT N_( \
    "If you check this box, IPv4 will be used by default for all UDP and " \
    "HTTP connections.")

#define ADEC_MPEG_TEXT N_("choose MPEG audio decoder")
#define ADEC_MPEG_LONGTEXT N_( \
    "This allows you to select the MPEG audio decoder you want to use. " \
    "Common choices are builtin and mad.")

#define ADEC_AC3_TEXT N_("choose AC3 audio decoder")
#define ADEC_AC3_LONGTEXT N_( \
    "This allows you to select the AC3/A52 audio decoder you want to use. " \
    "Common choices are builtin and a52.")

#define MMX_TEXT N_("enable CPU MMX support")
#define MMX_LONGTEXT N_( \
    "If your processor supports the MMX instructions set, vlc can take " \
    "advantage of them.")

#define THREE_DN_TEXT N_("enable CPU 3D Now! support")
#define THREE_DN_LONGTEXT N_( \
    "If your processor supports the 3D Now! instructions set, vlc can take "\
    "advantage of them.")

#define MMXEXT_TEXT N_("enable CPU MMX EXT support")
#define MMXEXT_LONGTEXT N_( \
    "If your processor supports the MMX EXT instructions set, vlc can take "\
    "advantage of them.")

#define SSE_TEXT N_("enable CPU SSE support")
#define SSE_LONGTEXT N_( \
    "If your processor supports the SSE instructions set, vlc can take " \
    "can take advantage of them.")

#define ALTIVEC_TEXT N_("enable CPU AltiVec support")
#define ALTIVEC_LONGTEXT N_( \
    "If your processor supports the AltiVec instructions set, vlc can take "\
    "advantage of them.")

#define PL_LAUNCH_TEXT N_("launch playlist on startup")
#define PL_LAUNCH_LONGTEXT N_( \
    "If you want vlc to start playing on startup, then enable this option.")

#define PL_ENQUEUE_TEXT N_("enqueue items in playlist")
#define PL_ENQUEUE_LONGTEXT N_( \
    "If you want vlc to add items to the playlist as you open them, then " \
    "enable this option.")

#define PL_LOOP_TEXT N_("loop playlist on end")
#define PL_LOOP_LONGTEXT N_( \
    "If you want vlc to keep playing the playlist indefinitely then enable " \
    "this option.")

#define MEMCPY_TEXT N_("memory copy module")
#define MEMCPY_LONGTEXT N_( \
    "You can select wich memory copy module you want to use. By default vlc " \
    "will select the fastest one supported by your hardware.")

#define ACCESS_TEXT N_("access module")
#define ACCESS_LONGTEXT N_( \
    "This is a legacy entry to let you configure access modules")

#define DEMUX_TEXT N_("demux module")
#define DEMUX_LONGTEXT N_( \
    "This is a legacy entry to let you configure demux modules")

#define FAST_MUTEX_TEXT N_("fast mutex on NT/2K/XP (developpers only)")
#define FAST_MUTEX_LONGTEXT N_( \
    "On Windows NT/2K/XP we use a slow mutex implementation but which " \
    "allows us to correctely implement condition variables. " \
    "You can also use the faster Win9x implementation but you might " \
    "experience problems with it.")

#define WIN9X_CV_TEXT N_("Condition variables implementation for Win9x " \
    "(developpers only)")
#define WIN9X_CV_LONGTEXT N_( \
    "On Windows 9x/Me we use a fast but not correct condition variables " \
    "implementation (more precisely there is a possibility for a race " \
    "condition to happen). " \
    "However it is possible to use slower alternatives which should be more " \
    "robust. " \
    "Currently you can choose between implementation 0 (which is the " \
    "default and the fastest), 1 and 2.")

/*
 * Quick usage guide for the configuration options:
 *
 * MODULE_CONFIG_START
 * MODULE_CONFIG_STOP
 * ADD_CATEGORY_HINT( N_(text), longtext )
 * ADD_SUBCATEGORY_HINT( N_(text), longtext )
 * ADD_STRING( option_name, value, p_callback, N_(text), N_(longtext) )
 * ADD_FILE( option_name, psz_value, p_callback, N_(text), N_(longtext) )
 * ADD_MODULE( option_name, psz_value, i_capability, p_callback,
 *             N_(text), N_(longtext) )
 * ADD_INTEGER( option_name, i_value, p_callback, N_(text), N_(longtext) )
 * ADD_BOOL( option_name, b_value, p_callback, N_(text), N_(longtext) )
 */

MODULE_CONFIG_START

/* Interface options */
ADD_CATEGORY_HINT( N_("Interface"), NULL)
ADD_MODULE_WITH_SHORT  ( "intf", 'I', MODULE_CAPABILITY_INTF, NULL, NULL, INTF_TEXT, INTF_LONGTEXT )
ADD_INTEGER ( "warning", 0, NULL, WARNING_TEXT, WARNING_LONGTEXT )
ADD_BOOL    ( "stats", 0, NULL, STATS_TEXT, STATS_LONGTEXT )
ADD_STRING  ( "search-path", NULL, NULL, INTF_PATH_TEXT, INTF_PATH_LONGTEXT )

/* Audio options */
ADD_CATEGORY_HINT( N_("Audio"), NULL)
ADD_MODULE_WITH_SHORT  ( "aout", 'A', MODULE_CAPABILITY_AOUT, NULL, NULL, AOUT_TEXT, AOUT_LONGTEXT )
ADD_BOOL    ( "audio", 1, NULL, AUDIO_TEXT, AUDIO_LONGTEXT )
ADD_BOOL    ( "mono", 0, NULL, MONO_TEXT, MONO_LONGTEXT )
ADD_INTEGER ( "volume", VOLUME_DEFAULT, NULL, VOLUME_TEXT, VOLUME_LONGTEXT )
ADD_INTEGER ( "rate", 44100, NULL, RATE_TEXT, RATE_LONGTEXT )
ADD_INTEGER ( "desync", 0, NULL, DESYNC_TEXT, DESYNC_LONGTEXT )
ADD_INTEGER ( "audio-format", 0, NULL, FORMAT_TEXT, FORMAT_LONGTEXT )

/* Video options */
ADD_CATEGORY_HINT( N_("Video"), NULL )
ADD_MODULE_WITH_SHORT  ( "vout", 'V', MODULE_CAPABILITY_VOUT, NULL, NULL, VOUT_TEXT, VOUT_LONGTEXT )
ADD_BOOL    ( "video", 1, NULL, VIDEO_TEXT, VIDEO_LONGTEXT )
ADD_INTEGER ( "width", -1, NULL, WIDTH_TEXT, WIDTH_LONGTEXT )
ADD_INTEGER ( "height", -1, NULL, HEIGHT_TEXT, HEIGHT_LONGTEXT )
ADD_FLOAT   ( "zoom", 1, NULL, ZOOM_TEXT, ZOOM_LONGTEXT )
ADD_BOOL    ( "grayscale", 0, NULL, GRAYSCALE_TEXT, GRAYSCALE_LONGTEXT )
ADD_BOOL    ( "fullscreen", 0, NULL, FULLSCREEN_TEXT, FULLSCREEN_LONGTEXT )
ADD_BOOL    ( "overlay", 1, NULL, OVERLAY_TEXT, OVERLAY_LONGTEXT )
ADD_INTEGER ( "spumargin", -1, NULL, SPUMARGIN_TEXT, SPUMARGIN_LONGTEXT )
ADD_MODULE  ( "filter", MODULE_CAPABILITY_VOUT, NULL, NULL, FILTER_TEXT, FILTER_LONGTEXT )

/* Input options */
ADD_CATEGORY_HINT( N_("Input"), NULL )
ADD_INTEGER ( "server-port", 1234, NULL, SERVER_PORT_TEXT, SERVER_PORT_LONGTEXT )
ADD_BOOL    ( "network-channel", 0, NULL, NETCHANNEL_TEXT, NETCHANNEL_LONGTEXT )
ADD_STRING  ( "channel-server", "localhost", NULL, CHAN_SERV_TEXT, CHAN_SERV_LONGTEXT )
ADD_INTEGER ( "channel-port", 6010, NULL, CHAN_PORT_TEXT, CHAN_PORT_LONGTEXT )
ADD_INTEGER ( "mtu", 1500, NULL, MTU_TEXT, MTU_LONGTEXT )
ADD_STRING  ( "iface", "eth0", NULL, IFACE_TEXT, IFACE_LONGTEXT )

ADD_INTEGER ( "program", 0, NULL, INPUT_PROGRAM_TEXT, INPUT_PROGRAM_LONGTEXT )
ADD_INTEGER ( "audio-type", -1, NULL, INPUT_AUDIO_TEXT, INPUT_AUDIO_LONGTEXT )
ADD_INTEGER ( "audio-channel", -1, NULL, INPUT_CHAN_TEXT, INPUT_CHAN_LONGTEXT )
ADD_INTEGER ( "spu-channel", -1, NULL, INPUT_SUBT_TEXT, INPUT_SUBT_LONGTEXT )

ADD_STRING  ( "dvd", DVD_DEVICE, NULL, DVD_DEV_TEXT, DVD_DEV_LONGTEXT )
ADD_STRING  ( "vcd", VCD_DEVICE, NULL, VCD_DEV_TEXT, VCD_DEV_LONGTEXT )

ADD_BOOL_WITH_SHORT ( "ipv6", '6', 0, NULL, IPV6_TEXT, IPV6_LONGTEXT )
ADD_BOOL_WITH_SHORT ( "ipv4", '4', 0, NULL, IPV4_TEXT, IPV4_LONGTEXT )

/* Decoder options */
ADD_CATEGORY_HINT( N_("Decoders"), NULL )
ADD_MODULE  ( "mpeg-adec", MODULE_CAPABILITY_DECODER, NULL, NULL, ADEC_MPEG_TEXT, ADEC_MPEG_LONGTEXT )
ADD_MODULE  ( "ac3-adec", MODULE_CAPABILITY_DECODER, NULL, NULL, ADEC_AC3_TEXT, ADEC_AC3_LONGTEXT )

/* CPU options */
ADD_CATEGORY_HINT( N_("CPU"), NULL )
ADD_BOOL ( "mmx", 1, NULL, MMX_TEXT, MMX_LONGTEXT )
ADD_BOOL ( "3dn", 1, NULL, THREE_DN_TEXT, THREE_DN_LONGTEXT )
ADD_BOOL ( "mmxext", 1, NULL, MMXEXT_TEXT, MMXEXT_LONGTEXT )
ADD_BOOL ( "sse", 1, NULL, SSE_TEXT, SSE_LONGTEXT )
ADD_BOOL ( "altivec", 1, NULL, ALTIVEC_TEXT, ALTIVEC_LONGTEXT )

/* Playlist options */
ADD_CATEGORY_HINT( N_("Playlist"), NULL )
ADD_BOOL ( "launch-playlist", 0, NULL, PL_LAUNCH_TEXT, PL_LAUNCH_LONGTEXT )
ADD_BOOL ( "enqueue-playlist", 0, NULL, PL_ENQUEUE_TEXT, PL_ENQUEUE_LONGTEXT )
ADD_BOOL ( "loop-playlist", 0, NULL, PL_LOOP_TEXT, PL_LOOP_LONGTEXT )

/* Misc options */
ADD_CATEGORY_HINT( N_("Miscellaneous"), NULL )
ADD_MODULE  ( "memcpy", MODULE_CAPABILITY_MEMCPY, NULL, NULL, MEMCPY_TEXT, MEMCPY_LONGTEXT )
ADD_MODULE  ( "access", MODULE_CAPABILITY_ACCESS, NULL, NULL, ACCESS_TEXT, ACCESS_LONGTEXT )
ADD_MODULE  ( "demux", MODULE_CAPABILITY_DEMUX, NULL, NULL, DEMUX_TEXT, DEMUX_LONGTEXT )

#if defined(WIN32)
ADD_BOOL ( "fast-mutex", 0, NULL, FAST_MUTEX_TEXT, FAST_MUTEX_LONGTEXT )
ADD_INTEGER ( "win9x-cv-method", 0, NULL, WIN9X_CV_TEXT, WIN9X_CV_LONGTEXT )
#endif

MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( N_("main program") )
    ADD_CAPABILITY( MAIN, 100/*whatever*/ )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/* Hack for help options */
static module_t help_module;
static module_config_t p_help_config[] =
{
    { MODULE_CONFIG_ITEM_BOOL, "help", 'h', N_("print help"),
      NULL, NULL, 0, 0, NULL, NULL, 0 },
    { MODULE_CONFIG_ITEM_BOOL, "longhelp", 'H', N_("print detailed help"),
      NULL, NULL, 0, 0, NULL, NULL, 0 },
    { MODULE_CONFIG_ITEM_BOOL, "list", 'l', N_("print a list of available "
      "modules"), NULL, NULL, 0, 0, NULL, NULL, 0 },
    { MODULE_CONFIG_ITEM_STRING, "module", 'p', N_("print help on module"),
      NULL, NULL, 0, 0, NULL, &help_module.config_lock, 0 },
    { MODULE_CONFIG_ITEM_BOOL, "version", '\0',
      N_("print version information"), NULL, NULL, 0, 0, NULL, NULL, 0 },
    { MODULE_CONFIG_HINT_END, NULL, '\0', NULL, NULL, NULL, 0, 0,
      NULL, NULL, 0 }
};

/*****************************************************************************
 * End configuration.
 *****************************************************************************/

/*****************************************************************************
 * Global variables - these are the only ones, see main.h and modules.h
 *****************************************************************************/
main_t        *p_main;
module_bank_t *p_module_bank;
input_bank_t  *p_input_bank;
aout_bank_t   *p_aout_bank;
vout_bank_t   *p_vout_bank;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  GetFilenames                ( int i_argc, char *ppsz_argv[] );
static void Usage                       ( const char *psz_module_name );
static void ListModules                 ( void );
static void Version                     ( void );

static void InitSignalHandler           ( void );
static void SimpleSignalHandler         ( int i_signal );
static void FatalSignalHandler          ( int i_signal );
static void IllegalSignalHandler        ( int i_signal );
static u32  CPUCapabilities             ( void );

#ifdef WIN32
static void ShowConsole                 ( void );
#endif

static jmp_buf env;
static int     i_illegal;
#if defined( __i386__ )
static char   *psz_capability;
#endif

/*****************************************************************************
 * main: parse command line, start interface and spawn threads
 *****************************************************************************
 * Steps during program execution are:
 *      -configuration parsing and messages interface initialization
 *      -opening of audio output device and some global modules
 *      -execution of interface, which exit on error or on user request
 *      -closing of audio output device and some global modules
 * On error, the spawned threads are canceled, and the open devices closed.
 *****************************************************************************/
int main( int i_argc, char *ppsz_argv[], char *ppsz_env[] )
{
    main_t        main_data;                /* root of all data - see main.h */
    module_bank_t module_bank;
    input_bank_t  input_bank;
    aout_bank_t   aout_bank;
    vout_bank_t   vout_bank;
    char *psz_module;
    char *p_tmp;

    p_main        = &main_data;               /* set up the global variables */
    p_module_bank = &module_bank;
    p_input_bank  = &input_bank;
    p_aout_bank   = &aout_bank;
    p_vout_bank   = &vout_bank;

    p_main->i_warning_level = 0;

    /*
     * Support for gettext
     */
#if defined( ENABLE_NLS ) && defined ( HAVE_GETTEXT )
#   if defined( HAVE_LOCALE_H ) && defined( HAVE_LC_MESSAGES )
    if( !setlocale( LC_MESSAGES, "" ) )
    {
        fprintf( stderr, "warning: unsupported locale settings\n" );
    }

    setlocale( LC_CTYPE, "" );
#   endif

    if( !bindtextdomain( PACKAGE, LOCALEDIR ) )
    {
        fprintf( stderr, "warning: no domain %s in directory %s\n",
                 PACKAGE, LOCALEDIR );
    }

    textdomain( PACKAGE );
#endif

    /*
     * Initialize threads system
     */
    vlc_threads_init();

    /*
     * Test if our code is likely to run on this CPU
     */
    p_main->i_cpu_capabilities = CPUCapabilities();

    /*
     * System specific initialization code
     */
#if defined( SYS_BEOS ) || defined( SYS_DARWIN ) || defined( WIN32 )
    system_Init( &i_argc, ppsz_argv, ppsz_env );

#elif defined( SYS_LINUX )
#   ifdef DEBUG
    /* Activate malloc checking routines to detect heap corruptions. */
    putenv( "MALLOC_CHECK_=2" );
    putenv( "GNOME_DISABLE_CRASH_DIALOG=1" );
#   endif
#endif

    /*
     * Initialize messages interface
     */
    intf_MsgCreate();

    intf_Msg( COPYRIGHT_MESSAGE "\n" );


    /* Get the executable name (similar to the basename command) */
    if( i_argc > 0 )
    {
        p_main->psz_arg0 = p_tmp = ppsz_argv[ 0 ];
        while( *p_tmp )
        {
            if( *p_tmp == '/' ) p_main->psz_arg0 = ++p_tmp;
            else ++p_tmp;
        }
    }
    else
    {
        p_main->psz_arg0 = "vlc";
    }

    /*
     * Initialize the module bank and and load the configuration of the main
     * module. We need to do this at this stage to be able to display a short
     * help if required by the user. (short help == main module options)
     */
    module_InitBank();
    module_LoadMain();

    /* Hack: insert the help module here */
    help_module.psz_name = "help";
    help_module.psz_longname = "help module";
    help_module.psz_program = NULL;
    help_module.pp_shortcuts[0] = NULL;
    help_module.i_capabilities = 0;
    help_module.i_cpu_capabilities = 0;
    help_module.p_config = p_help_config;
    vlc_mutex_init( &help_module.config_lock );
    help_module.i_config_items = sizeof(p_help_config)
                                  / sizeof(module_config_t);
    help_module.i_bool_items = help_module.i_config_items;
    help_module.next = p_module_bank->first;
    p_module_bank->first = &help_module;
    /* end hack */

    if( config_LoadCmdLine( &i_argc, ppsz_argv, 1 ) )
    {
        intf_MsgDestroy();
        return( errno );
    }

    /* Check for short help option */
    if( config_GetIntVariable( "help" ) )
    {
        intf_Msg( _("Usage: %s [options] [parameters] [file]...\n"),
                    p_main->psz_arg0 );

        Usage( "help" );
        Usage( "main" );
        return( -1 );
    }

    /* Check for version option */
    if( config_GetIntVariable( "version" ) )
    {
        Version();
        return( -1 );
    }

    /* Hack: remove the help module here */
    p_module_bank->first = help_module.next;
    /* end hack */

    /*
     * Load the builtins and plugins into the module_bank.
     * We have to do it before config_Load*() because this also gets the
     * list of configuration options exported by each module and loads their
     * default values.
     */
    module_LoadBuiltins();
    module_LoadPlugins();
    intf_WarnMsg( 2, "module: module bank initialized, found %i modules",
                  p_module_bank->i_count );

    /* Hack: insert the help module here */
    help_module.next = p_module_bank->first;
    p_module_bank->first = &help_module;
    /* end hack */

    /* Check for help on modules */
    if( (p_tmp = config_GetPszVariable( "module" )) )
    {
        Usage( p_tmp );
        free( p_tmp );
        return( -1 );
    }

    /* Check for long help option */
    if( config_GetIntVariable( "longhelp" ) )
    {
        Usage( NULL );
        return( -1 );
    }

    /* Check for module list option */
    if( config_GetIntVariable( "list" ) )
    {
        ListModules();
        return( -1 );
    }

    /* Hack: remove the help module here */
    p_module_bank->first = help_module.next;
    /* end hack */


    /*
     * Override default configuration with config file settings
     */
    vlc_mutex_init( &p_main->config_lock );
    p_main->psz_homedir = config_GetHomeDir();
    config_LoadConfigFile( NULL );

    /*
     * Override configuration with command line settings
     */
    if( config_LoadCmdLine( &i_argc, ppsz_argv, 0 ) )
    {
#ifdef WIN32
        ShowConsole();
        /* Pause the console because it's destroyed when we exit */
        intf_Msg( "The command line options couldn't be loaded, check that "
                  "they are valid.\nPress the RETURN key to continue..." );
        getchar();
#endif
        intf_MsgDestroy();
        return( errno );
    }


    /*
     * System specific configuration
     */
    system_Configure();

    /* p_main inititalization. FIXME ? */
    p_main->i_warning_level = config_GetIntVariable( "warning" );
    p_main->i_desync = config_GetIntVariable( "desync" ) * (mtime_t)1000;
    p_main->b_stats = config_GetIntVariable( "stats" );
    p_main->b_audio = config_GetIntVariable( "audio" );
    p_main->b_stereo= !config_GetIntVariable( "mono" );
    p_main->b_video = config_GetIntVariable( "video" );
    if( !config_GetIntVariable( "mmx" ) )
        p_main->i_cpu_capabilities &= ~CPU_CAPABILITY_MMX;
    if( !config_GetIntVariable( "3dn" ) )
        p_main->i_cpu_capabilities &= ~CPU_CAPABILITY_3DNOW;
    if( !config_GetIntVariable( "mmxext" ) )
        p_main->i_cpu_capabilities &= ~CPU_CAPABILITY_MMXEXT;
    if( !config_GetIntVariable( "sse" ) )
        p_main->i_cpu_capabilities &= ~CPU_CAPABILITY_SSE;
    if( !config_GetIntVariable( "altivec" ) )
        p_main->i_cpu_capabilities &= ~CPU_CAPABILITY_ALTIVEC;


    if( p_main->b_stats )
    {
        char p_capabilities[200];
        p_capabilities[0] = '\0';

#define PRINT_CAPABILITY( capability, string )                              \
        if( p_main->i_cpu_capabilities & capability )                       \
        {                                                                   \
            strncat( p_capabilities, string " ",                            \
                     sizeof(p_capabilities) - strlen(p_capabilities) );     \
            p_capabilities[sizeof(p_capabilities) - 1] = '\0';              \
        }

        PRINT_CAPABILITY( CPU_CAPABILITY_486, "486" );
        PRINT_CAPABILITY( CPU_CAPABILITY_586, "586" );
        PRINT_CAPABILITY( CPU_CAPABILITY_PPRO, "Pentium Pro" );
        PRINT_CAPABILITY( CPU_CAPABILITY_MMX, "MMX" );
        PRINT_CAPABILITY( CPU_CAPABILITY_3DNOW, "3DNow!" );
        PRINT_CAPABILITY( CPU_CAPABILITY_MMXEXT, "MMXEXT" );
        PRINT_CAPABILITY( CPU_CAPABILITY_SSE, "SSE" );
        PRINT_CAPABILITY( CPU_CAPABILITY_ALTIVEC, "Altivec" );
        PRINT_CAPABILITY( CPU_CAPABILITY_FPU, "FPU" );
        intf_StatMsg( "info: CPU has capabilities : %s", p_capabilities );
    }

    /*
     * Initialize playlist and get commandline files
     */
    p_main->p_playlist = intf_PlaylistCreate();
    if( !p_main->p_playlist )
    {
        intf_ErrMsg( "playlist error: playlist initialization failed" );
        intf_MsgDestroy();
        return( errno );
    }
    intf_PlaylistInit( p_main->p_playlist );

    /*
     * Get input filenames given as commandline arguments
     */
    GetFilenames( i_argc, ppsz_argv );

    /*
     * Initialize input, aout and vout banks
     */
    input_InitBank();
    aout_InitBank();
    vout_InitBank();

    /*
     * Choose the best memcpy module
     */
    psz_module = config_GetPszVariable( "memcpy" );
    p_main->p_memcpy_module = module_Need( MODULE_CAPABILITY_MEMCPY,
                                           psz_module, NULL );
    if( psz_module ) free( psz_module );
    if( p_main->p_memcpy_module == NULL )
    {
        intf_ErrMsg( "intf error: no suitable memcpy module, "
                     "using libc default" );
        p_main->pf_memcpy = memcpy;
    }
    else
    {
        p_main->pf_memcpy = p_main->p_memcpy_module->p_functions
                                  ->memcpy.functions.memcpy.pf_memcpy;
    }

    /*
     * Initialize shared resources and libraries
     */
    if( config_GetIntVariable( "network-channel" ) &&
        network_ChannelCreate() )
    {
        /* On error during Channels initialization, switch off channels */
        intf_ErrMsg( "intf error: channels initialization failed, "
                                 "deactivating channels" );
        config_PutIntVariable( "network-channel", 0 );
    }

    /*
     * Try to run the interface
     */
    p_main->p_intf = intf_Create();
    if( p_main->p_intf == NULL )
    {
        intf_ErrMsg( "intf error: interface initialization failed" );
    }
    else
    {
        /*
         * Set signal handling policy for all threads
         */
        InitSignalHandler();

        /*
         * This is the main loop
         */
        p_main->p_intf->pf_run( p_main->p_intf );

        /*
         * Finished, destroy the interface
         */
        intf_Destroy( p_main->p_intf );

        /*
         * Go back into channel 0 which is the network
         */
        if( config_GetIntVariable( "network-channel" ) && p_main->p_channel )
        {
            network_ChannelJoin( COMMON_CHANNEL );
        }
    }

    /*
     * Free input, aout and vout banks
     */
    input_EndBank();
    vout_EndBank();
    aout_EndBank();

    /*
     * Free playlist
     */
    intf_PlaylistDestroy( p_main->p_playlist );

    /*
     * Free allocated memory
     */
    if( p_main->p_memcpy_module != NULL )
    {
        module_Unneed( p_main->p_memcpy_module );
    }

    free( p_main->psz_homedir );

    /*
     * Free module bank
     */
    module_EndBank();

    /*
     * System specific cleaning code
     */
    system_End();

    /*
     * Terminate messages interface and program
     */
    intf_WarnMsg( 1, "intf: program terminated" );
    intf_MsgDestroy();

    /*
     * Stop threads system
     */
    vlc_threads_end( );

    return 0;
}


/* following functions are local */

/*****************************************************************************
 * GetFilenames: parse command line options which are not flags
 *****************************************************************************
 * Parse command line for input files.
 *****************************************************************************/
static int GetFilenames( int i_argc, char *ppsz_argv[] )
{
    int i_opt;

    /* We assume that the remaining parameters are filenames */
    for( i_opt = optind; i_opt < i_argc; i_opt++ )
    {
        intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END,
                          ppsz_argv[ i_opt ] );
    }

    return( 0 );
}

/*****************************************************************************
 * Usage: print program usage
 *****************************************************************************
 * Print a short inline help. Message interface is initialized at this stage.
 *****************************************************************************/
static void Usage( const char *psz_module_name )
{
#define FORMAT_STRING "      --%s%s%s%s%s%s%s %s%s"
    /* option name -------------'     | | | |  | |
     * <bra --------------------------' | | |  | |
     * option type or "" ---------------' | |  | |
     * ket> ------------------------------' |  | |
     * padding spaces ----------------------'  | |
     * comment --------------------------------' |
     * comment suffix ---------------------------'
     *
     * The purpose of having bra and ket is that we might i18n them as well.
     */
#define LINE_START 8
#define PADDING_SPACES 25
    module_t *p_module;
    module_config_t *p_item;
    char psz_spaces[PADDING_SPACES+LINE_START+1];
    char psz_format[sizeof(FORMAT_STRING)];

    memset( psz_spaces, ' ', PADDING_SPACES+LINE_START );
    psz_spaces[PADDING_SPACES+LINE_START] = '\0';

    strcpy( psz_format, FORMAT_STRING );

#ifdef WIN32
    ShowConsole();
#endif

    /* Enumerate the config for each module */
    for( p_module = p_module_bank->first ;
         p_module != NULL ;
         p_module = p_module->next )
    {
        boolean_t b_help_module = !strcmp( "help", p_module->psz_name );

        if( psz_module_name && strcmp( psz_module_name, p_module->psz_name ) )
            continue;

        /* ignore modules without config options */
        if( !p_module->i_config_items ) continue;

        /* print module name */
        intf_Msg( _("%s module options:\n"), p_module->psz_name );

        for( p_item = p_module->p_config;
             p_item->i_type != MODULE_CONFIG_HINT_END;
             p_item++ )
        {
            char *psz_bra = NULL, *psz_type = NULL, *psz_ket = NULL;
            char *psz_suf = "", *psz_prefix = NULL;
            int i;

            switch( p_item->i_type )
            {
            case MODULE_CONFIG_HINT_CATEGORY:
                intf_Msg( " %s", p_item->psz_text );
                break;

            case MODULE_CONFIG_ITEM_STRING:
            case MODULE_CONFIG_ITEM_FILE:
            case MODULE_CONFIG_ITEM_MODULE: /* We could also have "=<" here */
                psz_bra = " <"; psz_type = _("string"); psz_ket = ">";
                break;
            case MODULE_CONFIG_ITEM_INTEGER:
                psz_bra = " <"; psz_type = _("integer"); psz_ket = ">";
                break;
            case MODULE_CONFIG_ITEM_FLOAT:
                psz_bra = " <"; psz_type = _("float"); psz_ket = ">";
                break;
            case MODULE_CONFIG_ITEM_BOOL:
                psz_bra = ""; psz_type = ""; psz_ket = "";
                if( !b_help_module )
                    psz_suf = p_item->i_value ? _(" (default enabled)") :
                                                _(" (default disabled)");
                break;
            }

            /* Add short option */
            if( p_item->i_short )
            {
                psz_format[2] = '-';
                psz_format[3] = p_item->i_short;
                psz_format[4] = ',';
            }
            else
            {
                psz_format[2] = ' ';
                psz_format[3] = ' ';
                psz_format[4] = ' ';
            }

            if( psz_type )
            {
                i = PADDING_SPACES - strlen( p_item->psz_name )
                     - strlen( psz_bra ) - strlen( psz_type )
                     - strlen( psz_ket ) - 1;
                if( p_item->i_type == MODULE_CONFIG_ITEM_BOOL &&
                    !b_help_module )
                {
                    boolean_t b_dash = 0;
                    psz_prefix = p_item->psz_name;
                    while( *psz_prefix )
                    {
                        if( *psz_prefix++ == '-' )
                        {
                            b_dash = 1;
                            break;
                        }
                    }

                    if( b_dash )
                    {
                        psz_prefix = ", --no-";
                        i -= strlen( p_item->psz_name ) + strlen( ", --no-" );
                    }
                    else
                    {
                        psz_prefix = ", --no";
                        i -= strlen( p_item->psz_name ) + strlen( ", --no" );
                    }
                }

                if( i < 0 )
                {
                    i = 0;
                    psz_spaces[i] = '\n';
                }
                else
                {
                    psz_spaces[i] = '\0';
                }

                if( p_item->i_type == MODULE_CONFIG_ITEM_BOOL &&
                    !b_help_module )
                {
                    intf_Msg( psz_format, p_item->psz_name, psz_prefix,
                              p_item->psz_name, psz_bra, psz_type, psz_ket,
                              psz_spaces, p_item->psz_text, psz_suf );
                }
                else
                {
                    intf_Msg( psz_format, p_item->psz_name, "", "",
                              psz_bra, psz_type, psz_ket, psz_spaces,
                              p_item->psz_text, psz_suf );
                }
                psz_spaces[i] = ' ';
            }
        }

        /* Yet another nasty hack.
         * Maybe we could use MODULE_CONFIG_ITEM_END to display tail messages
         * for each module?? */
        if( !strcmp( "main", p_module->psz_name ) )
        {
            intf_Msg( _("\nPlaylist items:"
                "\n  *.mpg, *.vob                   plain MPEG-1/2 files"
                "\n  [dvd:][device][@raw_device][@[title][,[chapter][,angle]]]"
                "\n                                 DVD device"
                "\n  [vcd:][device][@[title][,[chapter]]"
                "\n                                 VCD device"
                "\n  udpstream:[@[<bind address>][:<bind port>]]"
                "\n                                 UDP stream sent by VLS"
                "\n  vlc:loop                       loop execution of the "
                      "playlist"
                "\n  vlc:pause                      pause execution of "
                      "playlist items"
                "\n  vlc:quit                       quit VLC") );
        }

        intf_Msg( "" );

    }

#ifdef WIN32        /* Pause the console because it's destroyed when we exit */
        intf_Msg( _("\nPress the RETURN key to continue...") );
        getchar();
#endif
}

/*****************************************************************************
 * ListModules: list the available modules with their description
 *****************************************************************************
 * Print a list of all available modules (builtins and plugins) and a short
 * description for each one.
 *****************************************************************************/
static void ListModules( void )
{
    module_t *p_module;
    char psz_spaces[22];

    memset( psz_spaces, ' ', 22 );

#ifdef WIN32
    ShowConsole();
#endif

    /* Usage */
    intf_Msg( _("Usage: %s [options] [parameters] [file]...\n"),
              p_main->psz_arg0 );

    intf_Msg( _("[module]              [description]") );

    /* Enumerate each module */
    for( p_module = p_module_bank->first ;
         p_module != NULL ;
         p_module = p_module->next )
    {
        int i;

        /* Nasty hack, but right now I'm too tired to think about a nice
         * solution */
        i = 22 - strlen( p_module->psz_name ) - 1;
        if( i < 0 ) i = 0;
        psz_spaces[i] = 0;

        intf_Msg( "  %s%s %s", p_module->psz_name, psz_spaces,
                  p_module->psz_longname );

        psz_spaces[i] = ' ';

    }

#ifdef WIN32        /* Pause the console because it's destroyed when we exit */
        intf_Msg( _("\nPress the RETURN key to continue...") );
        getchar();
#endif
}

/*****************************************************************************
 * Version: print complete program version
 *****************************************************************************
 * Print complete program version and build number.
 *****************************************************************************/
static void Version( void )
{
#ifdef WIN32
    ShowConsole();
#endif

    intf_Msg( VERSION_MESSAGE );
    intf_Msg(
      _("This program comes with NO WARRANTY, to the extent permitted by "
        "law.\nYou may redistribute it under the terms of the GNU General "
        "Public License;\nsee the file named COPYING for details.\n"
        "Written by the VideoLAN team at Ecole Centrale, Paris.") );

#ifdef WIN32        /* Pause the console because it's destroyed when we exit */
        intf_Msg( _("\nPress the RETURN key to continue...") );
        getchar();
#endif
}

/*****************************************************************************
 * InitSignalHandler: system signal handler initialization
 *****************************************************************************
 * Set the signal handlers. SIGTERM is not intercepted, because we need at
 * at least a method to kill the program when all other methods failed, and
 * when we don't want to use SIGKILL.
 *****************************************************************************/
static void InitSignalHandler( void )
{
    /* Termination signals */
#ifndef WIN32
    signal( SIGINT,  FatalSignalHandler );
    signal( SIGHUP,  FatalSignalHandler );
    signal( SIGQUIT, FatalSignalHandler );

    /* Other signals */
    signal( SIGALRM, SimpleSignalHandler );
    signal( SIGPIPE, SimpleSignalHandler );
#endif
}

/*****************************************************************************
 * SimpleSignalHandler: system signal handler
 *****************************************************************************
 * This function is called when a non fatal signal is received by the program.
 *****************************************************************************/
static void SimpleSignalHandler( int i_signal )
{
    /* Acknowledge the signal received */
    intf_WarnMsg( 0, "intf: ignoring signal %d", i_signal );
}

/*****************************************************************************
 * FatalSignalHandler: system signal handler
 *****************************************************************************
 * This function is called when a fatal signal is received by the program.
 * It tries to end the program in a clean way.
 *****************************************************************************/
static void FatalSignalHandler( int i_signal )
{
    static mtime_t abort_time = 0;
    static volatile boolean_t b_die = 0;

    /* Once a signal has been trapped, the termination sequence will be
     * armed and following signals will be ignored to avoid sending messages
     * to an interface having been destroyed */
    if( !b_die )
    {
        b_die = 1;
        abort_time = mdate();

        /* Acknowledge the signal received */
        intf_ErrMsg( "intf error: signal %d received, exiting - do it again "
                     "if vlc gets stuck", i_signal );

        /* Try to terminate everything - this is done by requesting the end
         * of the interface thread */
        p_main->p_intf->b_die = 1;

        return;
    }

    /* If user asks again 1 second later, die badly */
    if( mdate() > abort_time + 1000000 )
    {
#ifndef WIN32
        signal( SIGINT,  SIG_IGN );
        signal( SIGHUP,  SIG_IGN );
        signal( SIGQUIT, SIG_IGN );
#endif

        intf_ErrMsg( "intf error: user insisted too much, dying badly" );
        exit( 1 );
    }
}

/*****************************************************************************
 * IllegalSignalHandler: system signal handler
 *****************************************************************************
 * This function is called when an illegal instruction signal is received by
 * the program. We use this function to test OS and CPU capabilities
 *****************************************************************************/
static void IllegalSignalHandler( int i_signal )
{
    /* Acknowledge the signal received */
    i_illegal = 1;

#ifdef HAVE_SIGRELSE
    sigrelse( i_signal );
#endif

#if defined( __i386__ )
    fprintf( stderr, "warning: your CPU has %s instructions, but not your "
                     "operating system.\n", psz_capability );
    fprintf( stderr, "         some optimizations will be disabled unless "
                     "you upgrade your OS\n" );
#   if defined( SYS_LINUX )
    fprintf( stderr, "         (for instance Linux kernel 2.4.x or later)\n" );
#   endif
#endif

    longjmp( env, 1 );
}

/*****************************************************************************
 * CPUCapabilities: list the processors MMX support and other capabilities
 *****************************************************************************
 * This function is called to list extensions the CPU may have.
 *****************************************************************************/
static u32 CPUCapabilities( void )
{
    volatile u32 i_capabilities = CPU_CAPABILITY_NONE;

#if defined( SYS_DARWIN )
    struct host_basic_info hi;
    kern_return_t          ret;
    host_name_port_t       host;

    int i_size;
    char *psz_name, *psz_subname;

    i_capabilities |= CPU_CAPABILITY_FPU;

    /* Should 'never' fail? */
    host = mach_host_self();

    i_size = sizeof( hi ) / sizeof( int );
    ret = host_info( host, HOST_BASIC_INFO, ( host_info_t )&hi, &i_size );

    if( ret != KERN_SUCCESS )
    {
        fprintf( stderr, "error: couldn't get CPU information\n" );
        return( i_capabilities );
    }

    slot_name( hi.cpu_type, hi.cpu_subtype, &psz_name, &psz_subname );
    /* FIXME: need better way to detect newer proccessors.
     * could do strncmp(a,b,5), but that's real ugly */
    if( !strcmp(psz_name, "ppc7400") || !strcmp(psz_name, "ppc7450") )
    {
        i_capabilities |= CPU_CAPABILITY_ALTIVEC;
    }

    return( i_capabilities );

#elif defined( __i386__ )
    volatile unsigned int  i_eax, i_ebx, i_ecx, i_edx;
    volatile boolean_t     b_amd;

    /* Needed for x86 CPU capabilities detection */
#   define cpuid( a )                      \
        asm volatile ( "pushl %%ebx\n\t"   \
                       "cpuid\n\t"         \
                       "movl %%ebx,%1\n\t" \
                       "popl %%ebx\n\t"    \
                     : "=a" ( i_eax ),     \
                       "=r" ( i_ebx ),     \
                       "=c" ( i_ecx ),     \
                       "=d" ( i_edx )      \
                     : "a"  ( a )          \
                     : "cc" );

    i_capabilities |= CPU_CAPABILITY_FPU;

#   if defined( CAN_COMPILE_SSE ) || defined ( CAN_COMPILE_3DNOW )
    signal( SIGILL, IllegalSignalHandler );
#   endif

    /* test for a 486 CPU */
    asm volatile ( "pushl %%ebx\n\t"
                   "pushfl\n\t"
                   "popl %%eax\n\t"
                   "movl %%eax, %%ebx\n\t"
                   "xorl $0x200000, %%eax\n\t"
                   "pushl %%eax\n\t"
                   "popfl\n\t"
                   "pushfl\n\t"
                   "popl %%eax\n\t"
                   "movl %%ebx,%1\n\t"
                   "popl %%ebx\n\t"
                 : "=a" ( i_eax ),
                   "=r" ( i_ebx )
                 :
                 : "cc" );

    if( i_eax == i_ebx )
    {
#   if defined( CAN_COMPILE_SSE ) || defined ( CAN_COMPILE_3DNOW )
        signal( SIGILL, NULL );
#   endif
        return( i_capabilities );
    }

    i_capabilities |= CPU_CAPABILITY_486;

    /* the CPU supports the CPUID instruction - get its level */
    cpuid( 0x00000000 );

    if( !i_eax )
    {
#   if defined( CAN_COMPILE_SSE ) || defined ( CAN_COMPILE_3DNOW )
        signal( SIGILL, NULL );
#   endif
        return( i_capabilities );
    }

    /* FIXME: this isn't correct, since some 486s have cpuid */
    i_capabilities |= CPU_CAPABILITY_586;

    /* borrowed from mpeg2dec */
    b_amd = ( i_ebx == 0x68747541 ) && ( i_ecx == 0x444d4163 )
                    && ( i_edx == 0x69746e65 );

    /* test for the MMX flag */
    cpuid( 0x00000001 );

    if( ! (i_edx & 0x00800000) )
    {
#   if defined( CAN_COMPILE_SSE ) || defined ( CAN_COMPILE_3DNOW )
        signal( SIGILL, NULL );
#   endif
        return( i_capabilities );
    }

    i_capabilities |= CPU_CAPABILITY_MMX;

    if( i_edx & 0x02000000 )
    {
        i_capabilities |= CPU_CAPABILITY_MMXEXT;

#   ifdef CAN_COMPILE_SSE
        /* We test if OS supports the SSE instructions */
        psz_capability = "SSE";
        i_illegal = 0;
        if( setjmp( env ) == 0 )
        {
            /* Test a SSE instruction */
            __asm__ __volatile__ ( "xorps %%xmm0,%%xmm0\n" : : );
        }

        if( i_illegal == 0 )
        {
            i_capabilities |= CPU_CAPABILITY_SSE;
        }
#   endif
    }

    /* test for additional capabilities */
    cpuid( 0x80000000 );

    if( i_eax < 0x80000001 )
    {
#   if defined( CAN_COMPILE_SSE ) || defined ( CAN_COMPILE_3DNOW )
        signal( SIGILL, NULL );
#   endif
        return( i_capabilities );
    }

    /* list these additional capabilities */
    cpuid( 0x80000001 );

#   ifdef CAN_COMPILE_3DNOW
    if( i_edx & 0x80000000 )
    {
        psz_capability = "3D Now!";
        i_illegal = 0;
        if( setjmp( env ) == 0 )
        {
            /* Test a 3D Now! instruction */
            __asm__ __volatile__ ( "pfadd %%mm0,%%mm0\n" "femms\n" : : );
        }

        if( i_illegal == 0 )
        {
            i_capabilities |= CPU_CAPABILITY_3DNOW;
        }
    }
#   endif

    if( b_amd && ( i_edx & 0x00400000 ) )
    {
        i_capabilities |= CPU_CAPABILITY_MMXEXT;
    }

#   if defined( CAN_COMPILE_SSE ) || defined ( CAN_COMPILE_3DNOW )
    signal( SIGILL, NULL );
#   endif
    return( i_capabilities );

#elif defined( __powerpc__ )

    i_capabilities |= CPU_CAPABILITY_FPU;

#   ifdef CAN_COMPILE_ALTIVEC
    signal( SIGILL, IllegalSignalHandler );

    i_illegal = 0;
    if( setjmp( env ) == 0 )
    {
        asm volatile ("mtspr 256, %0\n\t"
                      "vand %%v0, %%v0, %%v0"
                      :
                      : "r" (-1));
    }

    if( i_illegal == 0 )
    {
        i_capabilities |= CPU_CAPABILITY_ALTIVEC;
    }

    signal( SIGILL, NULL );
#   endif

    return( i_capabilities );

#elif defined( __sparc__ )

    i_capabilities |= CPU_CAPABILITY_FPU;
    return( i_capabilities );

#else
    /* default behaviour */
    return( i_capabilities );

#endif
}

/*****************************************************************************
 * ShowConsole: On Win32, create an output console for debug messages
 *****************************************************************************
 * This function is useful only on Win32.
 *****************************************************************************/
#ifdef WIN32 /*  */
static void ShowConsole( void )
{
    AllocConsole();
    freopen( "CONOUT$", "w", stdout );
    freopen( "CONOUT$", "w", stderr );
    freopen( "CONIN$", "r", stdin );
    return;
}
#endif
