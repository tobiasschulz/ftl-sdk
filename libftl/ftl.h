/**
 * \file ftl.h - Public Interface for the FTL SDK
 *
 * Copyright (c) 2015 Michael Casadevall
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 **/

#ifndef __FTL_H
#define __FTL_H

#include <stdint.h>

#ifdef _WIN32
#	ifdef __FTL_INTERNAL
#		define FTL_API __declspec(dllexport)
#	else
#		define FTL_API __declspec(dllimport)
#	endif
#else
#	define FTL_API
#endif

FTL_API extern const int FTL_VERSION_MAJOR;
FTL_API extern const int FTL_VERSION_MINOR;
FTL_API extern const int FTL_VERSION_MAINTENANCE;

/*! \defgroup ftl_public Public Interfaces for libftl */

/*! \brief Status codes used by libftl
 *  \ingroup ftl_public
 */

typedef enum {
  FTL_SUCCESS, /**< Operation was successful */
  FTL_NON_ZERO_POINTER, /**< Function required a zero-ed pointer, but didn't get one */
  FTL_MALLOC_FAILURE, /**< memory allocation failed */
  FTL_DNS_FAILURE, /**< DNS probe failed */
  FTL_CONNECT_ERROR, /**< Failed to connect to ingest */
  FTL_INTERNAL_ERROR, /**< Got valid inputs, but FTL failed to complete the action due to internal failure */
  FTL_CONFIG_ERROR, /**< The configuration supplied was invalid or incomplete */
  FTL_STREAM_REJECTED, /**< Ingest rejected our connect command */
  FTL_NOT_ACTIVE_STREAM, /**< The function required an active stream and was passed an inactive one */
  FTL_UNAUTHORIZED, /**< Parameters were correct, but streamer not authorized to use FTL */
  FTL_AUDIO_SSRC_COLLISION, /**< The audio SSRC from this IP is currently in use */
  FTL_VIDEO_SSRC_COLLISION, /**< The video SSRC from this IP is currently in use */
  FTL_BAD_REQUEST, /**< Ingest didn't like our request. Should never happen */
  FTL_OLD_VERSION, /**< libftl needs to be updated */
  FTL_BAD_OR_INVALID_STREAM_KEY,
  FTL_UNSUPPORTED_MEDIA_TYPE,
  FTL_NOT_CONNECTED,
  FTL_ALREADY_CONNECTED,
  FTL_UNKNOWN_ERROR_CODE,
  FTL_STATUS_TIMEOUT,
  FTL_STATUS_MEDIA_QUEUE_FULL,
  FTL_STATUS_WAITING_FOR_KEY_FRAME
} ftl_status_t;

typedef enum {
  FTL_CONNECTION_DISCONNECTED,
  FTL_CONNECTION_RECONNECTED
} ftl_connection_status_t;

/*! \brief Video codecs supported by FTL
 *  \ingroug ftl_public
 */

typedef enum {
  FTL_VIDEO_NULL, /**< No video for this stream */
  FTL_VIDEO_VP8,  /**< Google's VP8 codec (recommended default) */
  FTL_VIDEO_H264
} ftl_video_codec_t;

/*! \brief Audio codecs supported by FTL
 *  \ingroup ftl_public
 */

typedef enum {
  FTL_AUDIO_NULL, /**< No audio for this stream */
  FTL_AUDIO_OPUS, /**< Xiph's Opus audio codec */
  FTL_AUDIO_AAC
} ftl_audio_codec_t;

typedef enum {
  FTL_AUDIO_DATA, 
  FTL_VIDEO_DATA
} ftl_media_type_t;

/*! \brief Log levels used by libftl; returned via logging callback
 *  \ingroup ftl_public
 */

typedef enum {
  FTL_LOG_CRITICAL,
  FTL_LOG_ERROR,
  FTL_LOG_WARN,
  FTL_LOG_INFO,
  FTL_LOG_DEBUG
} ftl_log_severity_t;

/*! \brief Function prototype for FTL logging callback
 * \ingroup ftl_public
 */

 typedef void (*ftl_logging_function_t)(ftl_log_severity_t log_level, const char * log_message);
  typedef void (*ftl_status_function_t)(ftl_connection_status_t status);

 typedef struct {
   char *ingest_hostname;
   char *stream_key;
   ftl_video_codec_t video_codec;
   int video_kbps; //used for the leaky bucket to smooth out packet flow rate, set to 0 to bypass
   float video_frame_rate; //TODO: add runtime detection mode of frame rate to simplify sdk
   ftl_audio_codec_t audio_codec;
   void *status_callback;
   ftl_logging_function_t log_func;
 } ftl_ingest_params_t;

 typedef struct {
	 void* priv;
 } ftl_handle_t;

 typedef enum {
	 FTL_STATUS_NONE,
	 FTL_STATUS_EVENT,
	 FTL_STATUS_VIDEO_PACKETS,
	 FTL_STATUS_AUDIO_PACKETS,
	 FTL_STATUS_VIDEO,
	 FTL_STATUS_AUDIO
 } ftl_status_types_t;

 typedef enum {
	 FTL_STATUS_EVENT_TYPE_UNKNOWN,
	 FTL_STATUS_EVENT_TYPE_CONNECTED,
	 FTL_STATUS_EVENT_TYPE_DISCONNECTED
 } ftl_status_event_types_t;

 typedef enum {
	 FTL_STATUS_EVENT_REASON_NONE,
	 FTL_STATUS_EVENT_REASON_NO_MEDIA,
	 FTL_STATUS_EVENT_REASON_API_REQUEST,
	 FTL_STATUS_EVENT_REASON_UNKNOWN,
 } ftl_status_event_reasons_t;

 typedef struct {
	 ftl_status_event_types_t type;
	 ftl_status_event_reasons_t reason;
 }ftl_status_event_msg_t;

 typedef struct {
	 int received;
	 int lost;
	 int recovered;
	 int late;
	 int average_pps;//average packets per second
 }ftl_packet_stats_msg_t;

 typedef struct {
	 int frames_sent;
	 int bytes_sent;
	 int average_fps;
	 int max_frame_size;
 }ftl_video_frame_stats_msg_t;

 /*status messages*/
 typedef struct {
	 ftl_status_types_t type;
	 union {
		 ftl_status_event_msg_t event;
		 ftl_packet_stats_msg_t pkt_stats;
		 ftl_video_frame_stats_msg_t video_stats;
	 } msg;
 }ftl_status_msg_t;

/*!
 * \ingroup ftl_public
 * \brief FTL Initialization
 *
 * Before using FTL, you must call ftl_init before making any additional calls
 * in this library. ftl_init initializes any submodules that FTL depends on such
 * as libsrtp. Under normal cirmstances, this function should never fail.
 *
 * On Windows, this function calls WSAStartup() to initialize Winsock. It is
 * the responsibility of the calling app to call WSACleanup() at application
 * shutdown as FTL can't safely call it (as your application may be using sockets
 * elsewhere
 *
 * @returns FTL_INIT_SUCCESS on successful initialization. Otherwise, returns
 * ftl_init_status_t enum with the failure state.
 */
FTL_API ftl_status_t ftl_init();

FTL_API ftl_status_t ftl_ingest_create(ftl_handle_t *ftl_handle, ftl_ingest_params_t *params);

FTL_API ftl_status_t ftl_ingest_connect(ftl_handle_t *ftl_handle);

FTL_API ftl_status_t ftl_ingest_get_status(ftl_handle_t *ftl_handle, ftl_status_msg_t *msg, int ms_timeout);

FTL_API ftl_status_t ftl_ingest_update_hostname(ftl_handle_t *ftl_handle, const char *ingest_hostname);
FTL_API ftl_status_t ftl_ingest_update_stream_key(ftl_handle_t *ftl_handle, const char *stream_key);

FTL_API ftl_status_t ftl_ingest_get_status(ftl_handle_t *ftl_handle, ftl_status_msg_t *msg, int ms_timeout);

FTL_API ftl_status_t ftl_ingest_disconnect(ftl_handle_t *ftl_handle);

FTL_API ftl_status_t ftl_ingest_destroy(ftl_handle_t *ftl_handle);

#endif // __FTL_H
