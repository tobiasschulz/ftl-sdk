#define __FTL_INTERNAL
#include "ftl.h"
#include "ftl_private.h"

#ifdef _WIN32
static DWORD WINAPI recv_thread(LPVOID data);
#else
static void *recv_thread(void *data);
#endif
static int _nack_init(ftl_media_component_common_t *media);
static int _nack_destroy(ftl_media_component_common_t *media);
static ftl_media_component_common_t *_media_lookup(ftl_stream_configuration_private_t *ftl, uint32_t ssrc);
static int _media_make_video_rtp_packet(ftl_stream_configuration_private_t *ftl, uint8_t *in, int in_len, uint8_t *out, int *out_len, int first_pkt);
static int _media_make_audio_rtp_packet(ftl_stream_configuration_private_t *ftl, uint8_t *in, int in_len, uint8_t *out, int *out_len);
static int _media_set_marker_bit(ftl_media_component_common_t *mc, uint8_t *in);
static int _media_send_packet(ftl_stream_configuration_private_t *ftl, uint32_t ssrc, uint16_t sn, int len);
static uint8_t* _media_get_empty_packet(ftl_stream_configuration_private_t *ftl, uint32_t ssrc, uint16_t sn, int *buf_len);

#ifndef _WIN32
#include <sys/time.h>
#define INVALID_SOCKET (int)0
#define SOCKET_ERROR (int)-1
typedef void *PVOID;
typedef void *HANDLE;
#endif

ftl_status_t media_init(ftl_stream_configuration_private_t *ftl) {

	ftl_media_config_t *media = &ftl->media;
	struct hostent *server = NULL;
	ftl_status_t status = FTL_SUCCESS;

	//Create a socket
	if ((media->media_socket = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
	{
		FTL_LOG(FTL_LOG_ERROR, "Could not create socket : %s", ftl_get_socket_error());
	}
	FTL_LOG(FTL_LOG_INFO, "Socket created");

	if ((server = gethostbyname(ftl->ingest_ip)) == NULL) {
		FTL_LOG(FTL_LOG_ERROR, "No such host as %s\n", ftl->ingest_ip);
		return FTL_DNS_FAILURE;
	}

	//Prepare the sockaddr_in structure
	media->server_addr.sin_family = AF_INET;
	memcpy((char *)&media->server_addr.sin_addr.s_addr, (char *)server->h_addr, server->h_length);
	media->server_addr.sin_port = htons(media->assigned_port);

	media->recv_thread_running = TRUE;
#ifdef _WIN32
	if( (media->recv_thread_handle = CreateThread(NULL,	0, recv_thread, ftl, 0, &media->recv_thread_id)) == NULL){
#else
	if ((pthread_create(&media->recv_thread, NULL, recv_thread, ftl)) != 0) {
#endif
		return FTL_MALLOC_FAILURE;
	}

	media->max_mtu = MAX_MTU;

	ftl_media_component_common_t *video_comp = &ftl->video.media_component;

	video_comp->nack_slots_initalized = FALSE;

	if( (status = _nack_init(video_comp)) != FTL_SUCCESS) {
		return status;
	}

	video_comp->timestamp = 0; //TODO: should start at a random value
	video_comp->timestamp_step = (uint32_t)(90000.f / ftl->video.frame_rate); //TODO: FIX THIS, wont work if 90K/x isnt evenly divisible, need to handle rounding error
	gettimeofday(&video_comp->stats, NULL);

	ftl_media_component_common_t *audio_comp = &ftl->audio.media_component;
	audio_comp->nack_slots_initalized = FALSE;

	if ((status = _nack_init(audio_comp)) != FTL_SUCCESS) {
		return status;
	}

	audio_comp->timestamp = 0;
	audio_comp->timestamp_step = 48000 / 50; //TODO: dont assume the step size for audio

	gettimeofday(&audio_comp->stats, NULL);

	return status;
}

ftl_status_t media_destroy(ftl_stream_configuration_private_t *ftl) {
	ftl_media_config_t *media = &ftl->media;
	struct hostent *server = NULL;
	ftl_status_t status = FTL_SUCCESS;

	ftl_close_socket(media->media_socket);

	media->recv_thread_running = FALSE;
#ifdef _WIN32
	WaitForSingleObject(media->recv_thread_id, INFINITE);
#else
	pthread_join(media->recv_thread, NULL);
#endif

	media->max_mtu = 0;

	ftl_media_component_common_t *video_comp = &ftl->video.media_component;

	_nack_destroy(video_comp);

	video_comp->timestamp = 0; //TODO: should start at a random value
	video_comp->timestamp_step = 0;

	ftl_media_component_common_t *audio_comp = &ftl->audio.media_component;

	_nack_destroy(audio_comp);

	audio_comp->timestamp = 0;
	audio_comp->timestamp_step = 0;
	
	return status;
}

ftl_status_t media_send_audio(ftl_stream_configuration_private_t *ftl, uint8_t *data, int32_t len) {
	ftl_media_component_common_t *mc = &ftl->audio.media_component;
	uint8_t nalu_type = 0;

	int pkt_len;
	int payload_size;
	int consumed = 0;

	int remaining = len;

	while (remaining > 0) {
		uint16_t sn = mc->seq_num;
		uint32_t ssrc = mc->ssrc;
		uint8_t *pkt_buf;
		pkt_buf = _media_get_empty_packet(ftl, ssrc, sn, &pkt_len);

		payload_size = _media_make_audio_rtp_packet(ftl, data, remaining, pkt_buf, &pkt_len);

		remaining -= payload_size;
		consumed += payload_size;
		data += payload_size;

		_media_send_packet(ftl, ssrc, sn, pkt_len);
	}

	return FTL_SUCCESS;
}

ftl_status_t media_send_video(ftl_stream_configuration_private_t *ftl, uint8_t *data, int32_t len, int end_of_frame){
	ftl_media_component_common_t *mc = &ftl->video.media_component;
	uint8_t nalu_type = 0;

	int pkt_len;
	int payload_size;
	int consumed = 0;

	int remaining = len;
	int first_fu = 1;

	nalu_type = data[0] & 0x1F;

	while (remaining > 0) {
		uint16_t sn = mc->seq_num;
		uint32_t ssrc = mc->ssrc;
		uint8_t *pkt_buf;
		pkt_buf = _media_get_empty_packet(ftl, ssrc, sn, &pkt_len);

		payload_size = _media_make_video_rtp_packet(ftl, data, remaining, pkt_buf, &pkt_len, first_fu);

		first_fu = 0;
		remaining -= payload_size;
		consumed += payload_size;
		data += payload_size;

		/*if all data has been consumed set marker bit*/
		if (remaining <= 0 && end_of_frame) { 
			_media_set_marker_bit(mc, pkt_buf);
		}

		_media_send_packet(ftl, ssrc, sn, pkt_len);
	}

	struct timeval now, delta;
	gettimeofday(&now, NULL);
	timeval_subtract(&delta, &now, &mc->stats);
	float stats_interval = timeval_to_ms(&delta);

	if (stats_interval > 5000) {
		ftl_status_msg_t status;

		mc->stats = now;

		status.msg.video_stats.average_fps = 60;
		status.msg.video_stats.bytes_sent = 1234;
		status.msg.video_stats.frames_sent = 600;

		status.type = FTL_STATUS_VIDEO;

		enqueue_status_msg(ftl, &status);
	}

	return FTL_SUCCESS;
}

static int _nack_init(ftl_media_component_common_t *media) {

	for (int i = 0; i < NACK_RB_SIZE; i++) {
		if ((media->nack_slots[i] = (nack_slot_t *)malloc(sizeof(nack_slot_t))) == NULL) {
			FTL_LOG(FTL_LOG_ERROR, "Failed to allocate memory for nack buffer\n");
			return FTL_MALLOC_FAILURE;
		}

		nack_slot_t *slot = media->nack_slots[i];
		
#ifdef _WIN32
		if ((slot->mutex = CreateMutex(NULL, FALSE, NULL)) == NULL) {
#else
		if (pthread_mutex_init(&slot->mutex, NULL) != 0) {
#endif
			FTL_LOG(FTL_LOG_ERROR, "Failed to allocate memory for nack buffer\n");
			return FTL_MALLOC_FAILURE;
		}

		slot->len = 0;
		slot->sn = -1;
		//slot->insert_time = 0;
	}

	media->nack_slots_initalized = TRUE;
	media->seq_num = 0; //TODO: should start at a random value

	return FTL_SUCCESS;
}

static int _nack_destroy(ftl_media_component_common_t *media) {

	for (int i = 0; i < NACK_RB_SIZE; i++) {
		if (media->nack_slots[i] != NULL) {
#ifdef _WIN32
			CloseHandle(media->nack_slots[i]->mutex);
#else
			pthread_mutex_destroy(&media->nack_slots[i]->mutex);
#endif
			free(media->nack_slots[i]);
			media->nack_slots[i] = NULL;
		}
	}

	return 0;
}

static ftl_media_component_common_t *_media_lookup(ftl_stream_configuration_private_t *ftl, uint32_t ssrc) {
	ftl_media_component_common_t *mc = NULL;


	/*check audio*/
	mc = &ftl->audio.media_component;
	if (mc->ssrc == ssrc) {
		return mc;
	}

	/*check video*/
	mc = &ftl->video.media_component;
	if (mc->ssrc == ssrc) {
		return mc;
	}

	return NULL;
}

static uint8_t* _media_get_empty_packet(ftl_stream_configuration_private_t *ftl, uint32_t ssrc, uint16_t sn, int *buf_len) {
	ftl_media_component_common_t *mc;

	if ((mc = _media_lookup(ftl, ssrc)) == NULL) {
		FTL_LOG(FTL_LOG_ERROR, "Unable to find ssrc %d\n", ssrc);
		return NULL;
	}

	/*map sequence number to slot*/
	nack_slot_t *slot = mc->nack_slots[sn % NACK_RB_SIZE];

#ifdef _WIN32
	WaitForSingleObject(slot->mutex, INFINITE);
#else
	pthread_mutex_lock(&slot->mutex);
#endif

	*buf_len = sizeof(slot->packet);
	return slot->packet;
}

static int _media_send_packet(ftl_stream_configuration_private_t *ftl, uint32_t ssrc, uint16_t sn, int len) {
	ftl_media_component_common_t *mc;
	int tx_len;

	if ((mc = _media_lookup(ftl, ssrc)) == NULL) {
		FTL_LOG(FTL_LOG_ERROR, "Unable to find ssrc %d\n", ssrc);
		return -1;
	}

	/*map sequence number to slot*/
	nack_slot_t *slot = mc->nack_slots[sn % NACK_RB_SIZE];

	slot->len = len;
	slot->sn = sn;
	gettimeofday(&slot->insert_time, NULL);

	if ((tx_len = sendto(ftl->media.media_socket, slot->packet, slot->len, 0, (struct sockaddr*) &ftl->media.server_addr, sizeof(struct sockaddr_in))) == SOCKET_ERROR)
	{
		FTL_LOG(FTL_LOG_ERROR, "sendto() failed with error: %s", ftl_get_socket_error());
	}

#ifdef _WIN32
	ReleaseMutex(slot->mutex);
#else
	pthread_mutex_unlock(&slot->mutex);
#endif

	return tx_len;
}

//TODO: i dont like how mutexes are being handled here...rethink this
static int _nack_resend_packet(ftl_stream_configuration_private_t *ftl, uint32_t ssrc, uint16_t sn) {
	ftl_media_component_common_t *mc;
	int tx_len;

	if ((mc = _media_lookup(ftl, ssrc)) == NULL) {
		FTL_LOG(FTL_LOG_ERROR, "Unable to find ssrc %d\n", ssrc);
		return -1;
	}

	/*map sequence number to slot*/
	nack_slot_t *slot = mc->nack_slots[sn % NACK_RB_SIZE];

#ifdef _WIN32
	WaitForSingleObject(slot->mutex, INFINITE);
#else
	pthread_mutex_lock(&slot->mutex);
#endif

	if (slot->sn != sn) {
		FTL_LOG(FTL_LOG_WARN, "[%d] expected sn %d in slot but found %d...discarding retransmit request\n", ssrc, sn, slot->sn);
#ifdef _WIN32
		ReleaseMutex(slot->mutex);
#else
		pthread_mutex_unlock(&slot->mutex);
#endif
		return 0;
	}

	int req_delay = 0;
	struct timeval delta, now;
	gettimeofday(&now, NULL);
	timeval_subtract(&delta, &now, &slot->insert_time);
	req_delay = timeval_to_ms(&delta);

	tx_len = _media_send_packet(ftl, ssrc, sn, slot->len);
	FTL_LOG(FTL_LOG_INFO, "[%d] resent sn %d, request delay was %d ms\n", ssrc, sn, req_delay);

	return tx_len;
}

static int _media_make_video_rtp_packet(ftl_stream_configuration_private_t *ftl, uint8_t *in, int in_len, uint8_t *out, int *out_len, int first_pkt) {
	uint8_t sbit, ebit;
	int frag_len;
	ftl_video_component_t *video = &ftl->video;
	ftl_media_component_common_t *mc = &video->media_component;

	sbit = first_pkt ? 1 : 0;
	ebit = (in_len + RTP_HEADER_BASE_LEN + RTP_FUA_HEADER_LEN) <= ftl->media.max_mtu;

	uint32_t rtp_header;

	rtp_header = htonl((2 << 30) | (mc->payload_type << 16) | mc->seq_num);
        uint32_t* out2 = (uint32_t*)out;
        *out2 = rtp_header;
        out2++;

        rtp_header = htonl(mc->timestamp);
        *out2 = rtp_header;
        out2++;
        
        rtp_header = htonl(mc->ssrc);
        *out2 = rtp_header;
        out2++;
		
        out = (uint8_t*)out2;

	mc->seq_num++;

	if (sbit && ebit) {
		sbit = ebit = 0;
		frag_len = in_len;
		*out_len = frag_len + RTP_HEADER_BASE_LEN;
		memcpy(out, in, frag_len);
	}
	else {

		if (sbit) {
			video->fua_nalu_type = in[0];
			in += 1;
			in_len--;
		}

		out[0] = (video->fua_nalu_type & 0x60) | 28;
		out[1] = (sbit << 7) | (ebit << 6) | (video->fua_nalu_type & 0x1F);

		out += 2;

		frag_len = ftl->media.max_mtu - RTP_HEADER_BASE_LEN - RTP_FUA_HEADER_LEN;

		if (frag_len > in_len) {
			frag_len = in_len;
		}

		memcpy(out, in, frag_len);

		*out_len = frag_len + RTP_HEADER_BASE_LEN + RTP_FUA_HEADER_LEN;
	}

	return frag_len + sbit;
}

static int _media_make_audio_rtp_packet(ftl_stream_configuration_private_t *ftl, uint8_t *in, int in_len, uint8_t *out, int *out_len) {
	int payload_len = in_len;

	uint32_t rtp_header;

	ftl_audio_component_t *audio = &ftl->audio;
	ftl_media_component_common_t *mc = &audio->media_component;

        rtp_header = htonl((2 << 30) | (1 << 23) | (mc->payload_type << 16) | mc->seq_num);
        uint32_t* out2 = (uint32_t*)out;
        *out2 = rtp_header;
        out2++;

        rtp_header = htonl(mc->timestamp);
        *out2 = rtp_header;
        out2++;
        
        rtp_header = htonl(mc->ssrc);
        *out2 = rtp_header;
        out2++;
        
        out = (uint8_t*)out2;

	mc->seq_num++;
	mc->timestamp += mc->timestamp_step;

	memcpy(out, in, payload_len);

	*out_len = payload_len + RTP_HEADER_BASE_LEN;

	return in_len;
}

static int _media_set_marker_bit(ftl_media_component_common_t *mc, uint8_t *in) {
	uint32_t rtp_header;

	rtp_header = ntohl(*((uint32_t*)in));
	rtp_header |= 1 << 23; /*set marker bit*/
	*((uint32_t*)in) = htonl(rtp_header);

	mc->timestamp += mc->timestamp_step;

	return 0;
}


/*handles rtcp packets from ingest including lost packet retransmission requests (nack)*/
#ifdef _WIN32
static DWORD WINAPI recv_thread(LPVOID data)
#else
static void *recv_thread(void *data)
#endif
{
	ftl_stream_configuration_private_t *ftl = (ftl_stream_configuration_private_t *)data;
	ftl_media_config_t *media = &ftl->media;
	int ret;
	unsigned char *buf;

#ifdef _WIN32
	if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL)) {
		FTL_LOG(FTL_LOG_WARN, "Failed to set recv_thread priority to THREAD_PRIORITY_TIME_CRITICAL\n");
	}
#endif

	if ((buf = (unsigned char*)malloc(MAX_PACKET_BUFFER)) == NULL) {
		FTL_LOG(FTL_LOG_ERROR, "Failed to allocate recv buffer\n");
		return NULL;
	}

#if 0
	if (ret >= 0 && recv_size > 0) {
		if (!discard_recv_data(stream, (size_t)recv_size))
			return -1;
	}
#endif
	//os_set_thread_name("ftl-stream: recv_thread");

	while (media->recv_thread_running) {

#ifdef _WIN32
		ret = recv(media->media_socket, buf, MAX_PACKET_BUFFER, 0);
#else
		ret = recv(media->media_socket, buf, MAX_PACKET_BUFFER, 0);
#endif
		if (ret <= 0) {
			continue;
		}

		int version, padding, feedbackType, ptype, length, ssrcSender, ssrcMedia;
		uint16_t snBase, blp, sn;
		int recv_len = ret;

		if (recv_len < 2) {
			FTL_LOG(FTL_LOG_WARN, "recv packet too small to parse, discarding\n");
			continue;
		}

		/*extract rtp header*/
		version = (buf[0] >> 6) & 0x3;
		padding = (buf[0] >> 5) & 0x1;
		feedbackType = buf[0] & 0x1F;
		ptype = buf[1];

		if (feedbackType == 1 && ptype == 205) {

			length = ntohs(*((uint16_t*)(buf + 2)));

			if (recv_len < ((length + 1) * 4)) {
				FTL_LOG(FTL_LOG_WARN, "reported len was %d but packet is only %d...discarding\n", recv_len, ((length + 1) * 4));
				continue;
			}

			ssrcSender = ntohl(*((uint32_t*)(buf + 4)));
			ssrcMedia = ntohl(*((uint32_t*)(buf + 8)));

			uint16_t *p = (uint16_t *)(buf + 12);

			for (int fci = 0; fci < (length - 2); fci++) {
				//request the first sequence number
				snBase = ntohs(*p++);
				_nack_resend_packet(ftl, ssrcMedia, snBase);
				blp = ntohs(*p++);
				if (blp) {
					for (int i = 0; i < 16; i++) {
						if ((blp & (1 << i)) != 0) {
							sn = snBase + i + 1;
							_nack_resend_packet(ftl, ssrcMedia, sn);
						}
					}
				}
			}
		}
	}

	free(buf);

	FTL_LOG(FTL_LOG_INFO, "Exited Recv Thread\n");

	return 0;
}
