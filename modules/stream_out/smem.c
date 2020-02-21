/*****************************************************************************
 * smem.c: stream output to memory buffer module
 *****************************************************************************
 * Copyright (C) 2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Christophe Courtaut <christophe.courtaut@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * How to use it
 *****************************************************************************
 *
 * You should use this module in combination with the transcode module, to get
 * raw datas from it. This module does not make any conversion at all, so you
 * need to use the transcode module for this purpose.
 *
 * For example, you can use smem as it :
 * --sout="#transcode{vcodec=RV24,acodec=s16l}:smem{smem-options}"
 *
 * Into each lock function (audio and video), you will have all the information
 * you need to allocate a buffer, so that this module will copy data in it.
 *
 * the video-data and audio-data pointers will be passed to lock/unlock function
 *
 ******************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_block.h>
#include <vlc_codec.h>
#include <vlc_aout.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define T_VIDEO_FORMAT_CALLBACK N_( "Video format callback" )
#define LT_VIDEO_FORMAT_CALLBACK N_( "Address of the video format callback function." )

#define T_VIDEO_POSTRENDER_CALLBACK N_( "Video postrender callback" )
#define LT_VIDEO_POSTRENDER_CALLBACK N_( "Address of the video postrender callback function. " \
                                        "This function will be called when the render is into the buffer." )

#define T_AUDIO_FORMAT_CALLBACK N_( "Audio format callback" )
#define LT_AUDIO_FORMAT_CALLBACK N_( "Address of the audio format callback function." )

#define T_AUDIO_POSTRENDER_CALLBACK N_( "Audio postrender callback" )
#define LT_AUDIO_POSTRENDER_CALLBACK N_( "Address of the audio postrender callback function. " \
                                        "This function will be called when the render is into the buffer." )

#define T_VIDEO_DATA N_( "Video Callback data" )
#define LT_VIDEO_DATA N_( "Data for the video callback function." )

#define T_AUDIO_DATA N_( "Audio callback data" )
#define LT_AUDIO_DATA N_( "Data for the audio callback function." )

#define T_TIME_SYNC N_( "Time Synchronized output" )
#define LT_TIME_SYNC N_( "Time Synchronisation option for output. " \
                        "If true, stream will render as usual, else " \
                        "it will be rendered as fast as possible.")

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-smem-"
#define SOUT_PREFIX_VIDEO SOUT_CFG_PREFIX"video-"
#define SOUT_PREFIX_AUDIO SOUT_CFG_PREFIX"audio-"

vlc_module_begin ()
    set_shortname( N_("Smem"))
    set_description( N_("Stream output to memory buffer") )
    set_capability( "sout stream", 0 )
    add_shortcut( "smem" )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_STREAM )
    add_string( SOUT_PREFIX_VIDEO "format-callback", "0", T_VIDEO_FORMAT_CALLBACK, LT_VIDEO_FORMAT_CALLBACK, true )
        change_volatile()
    add_string( SOUT_PREFIX_AUDIO "format-callback", "0", T_AUDIO_FORMAT_CALLBACK, LT_AUDIO_FORMAT_CALLBACK, true )
        change_volatile()
    add_string( SOUT_PREFIX_VIDEO "postrender-callback", "0", T_VIDEO_POSTRENDER_CALLBACK, LT_VIDEO_POSTRENDER_CALLBACK, true )
        change_volatile()
    add_string( SOUT_PREFIX_AUDIO "postrender-callback", "0", T_AUDIO_POSTRENDER_CALLBACK, LT_AUDIO_POSTRENDER_CALLBACK, true )
        change_volatile()
    add_string( SOUT_PREFIX_VIDEO "data", "0", T_VIDEO_DATA, LT_VIDEO_DATA, true )
        change_volatile()
    add_string( SOUT_PREFIX_AUDIO "data", "0", T_AUDIO_DATA, LT_VIDEO_DATA, true )
        change_volatile()
    add_bool( SOUT_CFG_PREFIX "time-sync", true, T_TIME_SYNC, LT_TIME_SYNC, true )
        change_private()
    set_callbacks( Open, Close )
vlc_module_end ()


/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static const char *const ppsz_sout_options[] = {
    "video-format-callback", "audio-format-callback", "video-postrender-callback", "audio-postrender-callback", "video-data", "audio-data", "time-sync", NULL
};

static sout_stream_id_sys_t *Add( sout_stream_t *, const es_format_t * );
static void              Del ( sout_stream_t *, sout_stream_id_sys_t * );
static int               Send( sout_stream_t *, sout_stream_id_sys_t *, block_t* );

static sout_stream_id_sys_t *AddVideo( sout_stream_t *p_stream,
                                       const es_format_t *p_fmt );
static sout_stream_id_sys_t *AddAudio( sout_stream_t *p_stream,
                                       const es_format_t *p_fmt );

static int SendVideo( sout_stream_t *p_stream, sout_stream_id_sys_t *id,
                      block_t *p_buffer );
static int SendAudio( sout_stream_t *p_stream, sout_stream_id_sys_t *id,
                      block_t *p_buffer );

struct sout_stream_id_sys_t
{
    es_format_t format;
    void *p_data;
};

struct sout_stream_sys_t
{
    vlc_mutex_t *p_lock;
    void ( *pf_video_format_callback ) ( void* p_video_data, uint8_t* p_video_buffer, size_t size );
    void ( *pf_audio_format_callback ) ( void* p_audio_data, uint8_t* p_audio_buffer, size_t size );
    void ( *pf_video_postrender_callback ) ( void* p_video_data, uint8_t* p_video_buffer, size_t size, mtime_t pts, uint32_t flags );
    void ( *pf_audio_postrender_callback ) ( void* p_audio_data, uint8_t* p_audio_buffer, size_t size, mtime_t pts, uint32_t flags );
    bool time_sync;
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    char* psz_tmp;
    sout_stream_t *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys;

    p_sys = calloc( 1, sizeof( sout_stream_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;
    p_stream->p_sys = p_sys;

    config_ChainParse( p_stream, SOUT_CFG_PREFIX, ppsz_sout_options,
                       p_stream->p_cfg );

    p_sys->time_sync = var_GetBool( p_stream, SOUT_CFG_PREFIX "time-sync" );

    psz_tmp = var_GetString( p_stream, SOUT_PREFIX_VIDEO "format-callback" );
    p_sys->pf_video_format_callback = (void (*) (void*, uint8_t*, size_t))(intptr_t)atoll( psz_tmp );
    free( psz_tmp );

    psz_tmp = var_GetString( p_stream, SOUT_PREFIX_AUDIO "format-callback" );
    p_sys->pf_audio_format_callback = (void (*) (void*, uint8_t*, size_t))(intptr_t)atoll( psz_tmp );
    free( psz_tmp );

    psz_tmp = var_GetString( p_stream, SOUT_PREFIX_VIDEO "postrender-callback" );
    p_sys->pf_video_postrender_callback = (void (*) (void*, uint8_t*, size_t, mtime_t, uint32_t))(intptr_t)atoll( psz_tmp );
    free( psz_tmp );

    psz_tmp = var_GetString( p_stream, SOUT_PREFIX_AUDIO "postrender-callback" );
    p_sys->pf_audio_postrender_callback = (void (*) (void*, uint8_t*, size_t, mtime_t, uint32_t))(intptr_t)atoll( psz_tmp );
    free( psz_tmp );

    /* Setting stream out module callbacks */
    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;
    p_stream->pace_nocontrol = p_sys->time_sync;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_stream_t *p_stream = (sout_stream_t*)p_this;
    free( p_stream->p_sys );
}

static sout_stream_id_sys_t *Add( sout_stream_t *p_stream,
                                  const es_format_t *p_fmt )
{
    sout_stream_id_sys_t *id = NULL;

    if ( p_fmt->i_cat == VIDEO_ES )
        id = AddVideo( p_stream, p_fmt );
    else if ( p_fmt->i_cat == AUDIO_ES )
        id = AddAudio( p_stream, p_fmt );
    return id;
}

static sout_stream_id_sys_t *AddVideo( sout_stream_t *p_stream,
                                       const es_format_t *p_fmt )
{
    char* psz_tmp;
    sout_stream_id_sys_t    *id;

    id = calloc( 1, sizeof( sout_stream_id_sys_t ) );
    if( !id )
        return NULL;

    psz_tmp = var_GetString( p_stream, SOUT_PREFIX_VIDEO "data" );
    id->p_data = (void *)( intptr_t )atoll( psz_tmp );
    free( psz_tmp );

    es_format_Copy( &id->format, p_fmt );

    sout_stream_sys_t *p_sys = p_stream->p_sys;
    if ( p_sys->pf_video_format_callback != NULL )
    {
        p_sys->pf_video_format_callback( id->p_data, id->format.p_extra, id->format.i_extra );
    }

    return id;
}

static sout_stream_id_sys_t *AddAudio( sout_stream_t *p_stream,
                                       const es_format_t *p_fmt )
{
    char* psz_tmp;
    sout_stream_id_sys_t* id;

    id = calloc( 1, sizeof( sout_stream_id_sys_t ) );
    if( !id )
        return NULL;

    psz_tmp = var_GetString( p_stream, SOUT_PREFIX_AUDIO "data" );
    id->p_data = (void *)( intptr_t )atoll( psz_tmp );
    free( psz_tmp );

    es_format_Copy( &id->format, p_fmt );

    sout_stream_sys_t *p_sys = p_stream->p_sys;
    if ( p_sys->pf_audio_format_callback != NULL )
    {
        p_sys->pf_audio_format_callback( id->p_data, id->format.p_extra, id->format.i_extra );
    }

    return id;
}

static void Del( sout_stream_t *p_stream, sout_stream_id_sys_t *id )
{
    VLC_UNUSED( p_stream );
    es_format_Clean( &id->format );
    free( id );
}

static int Send( sout_stream_t *p_stream, sout_stream_id_sys_t *id,
                 block_t *p_buffer )
{
    if ( id->format.i_cat == VIDEO_ES )
        return SendVideo( p_stream, id, p_buffer );
    else if ( id->format.i_cat == AUDIO_ES )
        return SendAudio( p_stream, id, p_buffer );
    return VLC_SUCCESS;
}

static int SendVideo( sout_stream_t *p_stream, sout_stream_id_sys_t *id,
                      block_t *p_buffer )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    size_t i_size = p_buffer->i_buffer;

    /* Calling the postrender callback to tell the user his buffer is ready */
    if (p_sys->pf_video_postrender_callback != NULL)
    {
        p_sys->pf_video_postrender_callback( id->p_data, p_buffer->p_buffer,
                                            i_size, p_buffer->i_pts, p_buffer->i_flags );
    }

    block_ChainRelease( p_buffer );
    return VLC_SUCCESS;
}

static int SendAudio( sout_stream_t *p_stream, sout_stream_id_sys_t *id,
                      block_t *p_buffer )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    int i_size;

    i_size = p_buffer->i_buffer;

    /* Calling the postrender callback to tell the user his buffer is ready */
    if ( p_sys->pf_audio_postrender_callback != NULL )
    {
        p_sys->pf_audio_postrender_callback( id->p_data, p_buffer->p_buffer,
                                            i_size, p_buffer->i_pts, p_buffer->i_flags );
    }

    block_ChainRelease( p_buffer );
    return VLC_SUCCESS;
}

