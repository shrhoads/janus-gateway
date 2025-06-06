/*! \file   janus_nosip.c
 * \author Lorenzo Miniero <lorenzo@meetecho.com>
 * \copyright GNU General Public License v3
 * \brief  Janus NoSIP plugin
 * \details Check the \ref nosip for more details.
 *
 * \ingroup plugins
 * \ref plugins
 *
 * \page nosip NoSIP plugin documentation
 *
 * This is quite a basic plugin, as it only takes care of acting as an
 * RTP bridge. It is named "NoSIP" since, as the name suggests, signalling
 * takes no place here, and is entirely up to the application. The typical
 * usage of this application is something like this:
 *
 * 1. a WebRTC application handles signalling on its own (e.g., SIP), but
 * needs to interact with a peer that doesn't support WebRTC (DTLS/ICE);
 * 2. it creates a handle with the NoSIP plugin, creates a JSEP SDP offer,
 * and passes it to the plugin;
 * 3. the plugin creates a barebone SDP that can be used to communicate
 * with the legacy peer, binds to the ports for RTP/RTCP, and sends this
 * plain SDP back to the application;
 * 4. the application uses this barebone SDP in its signalling, and expects
 * an answer from the peer;
 * 5. the SDP answer from the peer will be barebone as well, and so unfit
 * for WebRTC usage; as such, the application passes it to the plugin as
 * the answer to match the offer created before;
 * 6. the plugin matches the answer to the offer, and starts exchanging
 * RTP/RTCP with the legacy peer: media coming from the peer is relayed
 * via WebRTC to the application, and WebRTC stuff coming from the application
 * is relayed via plain RTP/RTCP to the legacy peer.
 *
 * The same behaviour can be followed if the application is the callee
 * instead, with the only difference being that the barebone offer will
 * come from the peer in this case, and the application will ask the
 * NoSIP plugin for a barebone answer instead.
 *
 * As you can see, the behaviour is pretty much the same as the SIP plugin,
 * with the key difference being that in this case there's no SIP stack in
 * the plugin itself. All signalling is left to the application, and Janus
 * (via the NoSIP plugin) is only responsible for bridging the media. This
 * might be more appropriate than the SIP plugin in cases where developers
 * want to keep control on the signalling layer, while still involving a
 * server of sorts. Of course, SIP is just an example here: other signalling
 * protocols may be involved as well (e.g., IAX, XMPP, others). The NoSIP
 * plugin, though, will generate and expect plain SDP, so you'll need to
 * take care of any adaptation that may be needed to make this work with
 * the signalling protocol of your choice.
 *
 * \section nosipapi NoSIP Plugin API
 *
 * The plugin mainly supports two requests, \c generate and \c process,
 * which are both asynchronous. The \c generate request take a JSEP offer
 * or answer, and generates a barebone SDP the "legacy" application can
 * use; the \c process request, on the other hand, processes a remote
 * barebone SDP, and matches it to the plugin may have generated before,
 * in order to then return a JSEP offer or answer that can be used to
 * setup a PeerConnection.
 *
 * The \c generate request must be formatted as follows:
 *
\verbatim
{
	"request" : "generate",
	"info" : "<opaque string that the user can provide for context; optional>",
	"srtp" : "<whether to mandate (sdes_mandatory) or offer (sdes_optional) SRTP support; optional>",
	"srtp_profile" : "<SRTP profile to negotiate, in case SRTP is offered; optional>"
}
\endverbatim
 *
 * As anticipated, this requires a JSEP offer or answer passed via Janus
 * API as part of a WebRTC PeerConnection negotiation. If the conversion
 * of the WebRTC JSEP SDP to barebone SDP is successful, a \c generated
 * event is sent back to the user:
 *
\verbatim
{
	"event" : "generated",
	"type" : "<offer|answer, depending on the nature of the provided JSEP>",
	"sdp" : "<barebone SDP content>"
}
\endverbatim
 *
 * The \c process request, instead, must be formatted as follows:
 *
\verbatim
{
	"request" : "process",
	"type" : "<offer|answer, depending on the nature of the provided SDP>",
	"sdp" : "<barebone SDP to convert>"
	"info" : "<opaque string that the user can provide for context; optional>",
	"srtp" : "<whether to mandate (sdes_mandatory) or offer (sdes_optional) SRTP support; optional>",
	"srtp_profile" : "<SRTP profile to negotiate, in case SRTP is offered; optional>"
}
\endverbatim
 *
 * As anticipated, this requires a "legacy" SDP offer or answer passed via
 * NoSIP plugin messaging, which is why the caller must specify if it's an
 * offer or answer. If the request is successful, a \c processed event is
 * sent back to the user, along to the JSEP offer or answer that Janus
 * generated out of the barebone SDP:
 *
\verbatim
{
	"event" : "processed",
	"srtp" : "<whether the barebone SDP mandates (sdes_mandatory) or offers (sdes_optional) SRTP support; optional>"
}
\endverbatim
 *
 * To close a session you can use the \c hangup request, which needs no
 * additional arguments, as the whole context can be extracted from the
 * current state of the session in the plugin:
 *
\verbatim
{
	"request" : "hangup"
}
\endverbatim
 *
 * An \c hangingup event will be sent back, as this is an asynchronous request.
 *
 * Finally, just as in the SIP plugin, the multimedia session
 * can be recorded. Considering the NoSIP plugin also assumes two peers
 * are in a call with each other (although it makes no assumptions on
 * the signalling that ties them together), it works exactly the same
 * way as the SIP plugin dooes when it comes to recording.
 * Specifically, you make use of the \c recording request to either start
 * or stop a recording, using the following syntax:
 *
\verbatim
{
	"request" : "recording",
	"action" : "<start|stop, depending on whether you want to start or stop recording something>"
	"audio" : <true|false; whether or not our audio should be recorded>,
	"video" : <true|false; whether or not our video should be recorded>,
	"peer_audio" : <true|false; whether or not our peer's audio should be recorded>,
	"peer_video" : <true|false; whether or not our peer's video should be recorded>,
	"filename" : "<base path/filename to use for all the recordings>"
}
\endverbatim
 *
 * As you can see, this means that the two sides of conversation are recorded
 * separately, and so are the audio and video streams if available. You can
 * choose which ones to record, in case you're interested in just a subset.
 * The \c filename part is just a prefix, and dictates the actual filenames
 * that will be used for the up-to-four recordings that may need to be enabled.
 *
 * A \c recordingupdated event is sent back in case the request is successful.
 *
 * To programmatically send a video keyframe request to either the WebRTC user
 * or the SIP peer (or both), the \c keyframe request can be used. This
 * request is particularly useful when the SIP peer doesn't support RTCP PLI,
 * and so may use other mechanisms (e.g., via signalling) to ask for a keyframe
 * to get video working. By using this request, the WebRTC user can ask Janus
 * to originate a PLI programmatically. The direction of the keyframe request
 * can be provided by using the \c user and \c peer properties: if \c user
 * is \c TRUE a keyframe request will be sent by Janus to the WebRTC user;
 * if \c peer is \c TRUE a keyframe request will be sent by Janus to the
 * SIP peer. In both cases an RTCP PLI message will be sent. The syntax of
 * the message is the following:
 *
\verbatim
{
	"request" : "keyframe",
	"user" : <true|false; whether or not to send a keyframe request to the WebRTC user>,
	"peer" : <true|false; whether or not to send a keyframe request to the SIP peer>
}
\endverbatim
 *
 * A \c keyframesent event is sent back in case the request is successful.
 */

#include "plugin.h"

#include <arpa/inet.h>
#include <net/if.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>

#include <jansson.h>

#include "../debug.h"
#include "../apierror.h"
#include "../config.h"
#include "../mutex.h"
#include "../record.h"
#include "../rtp.h"
#include "../rtpsrtp.h"
#include "../rtcp.h"
#include "../ip-utils.h"
#include "../sdp-utils.h"
#include "../utils.h"


/* Plugin information */
#define JANUS_NOSIP_VERSION			1
#define JANUS_NOSIP_VERSION_STRING	"0.0.1"
#define JANUS_NOSIP_DESCRIPTION		"This is a simple RTP bridging plugin that leaves signalling details (e.g., SIP) up to the application."
#define JANUS_NOSIP_NAME			"JANUS NoSIP plugin"
#define JANUS_NOSIP_AUTHOR			"Meetecho s.r.l."
#define JANUS_NOSIP_PACKAGE			"janus.plugin.nosip"

/* Plugin methods */
janus_plugin *create(void);
int janus_nosip_init(janus_callbacks *callback, const char *config_path);
void janus_nosip_destroy(void);
int janus_nosip_get_api_compatibility(void);
int janus_nosip_get_version(void);
const char *janus_nosip_get_version_string(void);
const char *janus_nosip_get_description(void);
const char *janus_nosip_get_name(void);
const char *janus_nosip_get_author(void);
const char *janus_nosip_get_package(void);
void janus_nosip_create_session(janus_plugin_session *handle, int *error);
struct janus_plugin_result *janus_nosip_handle_message(janus_plugin_session *handle, char *transaction, json_t *message, json_t *jsep);
void janus_nosip_setup_media(janus_plugin_session *handle);
void janus_nosip_incoming_rtp(janus_plugin_session *handle, janus_plugin_rtp *packet);
void janus_nosip_incoming_rtcp(janus_plugin_session *handle, janus_plugin_rtcp *packet);
void janus_nosip_hangup_media(janus_plugin_session *handle);
void janus_nosip_destroy_session(janus_plugin_session *handle, int *error);
json_t *janus_nosip_query_session(janus_plugin_session *handle);

/* Plugin setup */
static janus_plugin janus_nosip_plugin =
	JANUS_PLUGIN_INIT (
		.init = janus_nosip_init,
		.destroy = janus_nosip_destroy,

		.get_api_compatibility = janus_nosip_get_api_compatibility,
		.get_version = janus_nosip_get_version,
		.get_version_string = janus_nosip_get_version_string,
		.get_description = janus_nosip_get_description,
		.get_name = janus_nosip_get_name,
		.get_author = janus_nosip_get_author,
		.get_package = janus_nosip_get_package,

		.create_session = janus_nosip_create_session,
		.handle_message = janus_nosip_handle_message,
		.setup_media = janus_nosip_setup_media,
		.incoming_rtp = janus_nosip_incoming_rtp,
		.incoming_rtcp = janus_nosip_incoming_rtcp,
		.hangup_media = janus_nosip_hangup_media,
		.destroy_session = janus_nosip_destroy_session,
		.query_session = janus_nosip_query_session,
	);

/* Plugin creator */
janus_plugin *create(void) {
	JANUS_LOG(LOG_VERB, "%s created!\n", JANUS_NOSIP_NAME);
	return &janus_nosip_plugin;
}

/* Parameter validation */
static struct janus_json_parameter request_parameters[] = {
	{"request", JSON_STRING, JANUS_JSON_PARAM_REQUIRED}
};
static struct janus_json_parameter generate_parameters[] = {
	{"info", JSON_STRING, 0},
	{"srtp", JSON_STRING, 0},
	{"srtp_profile", JSON_STRING, 0},
	{"update", JANUS_JSON_BOOL, 0}
};
static struct janus_json_parameter process_parameters[] = {
	{"type", JSON_STRING, JANUS_JSON_PARAM_REQUIRED},
	{"sdp", JSON_STRING, JANUS_JSON_PARAM_REQUIRED},
	{"info", JSON_STRING, 0},
	{"srtp", JSON_STRING, 0},
	{"srtp_profile", JSON_STRING, 0},
	{"update", JANUS_JSON_BOOL, 0}
};
static struct janus_json_parameter recording_parameters[] = {
	{"action", JSON_STRING, JANUS_JSON_PARAM_REQUIRED},
	{"audio", JANUS_JSON_BOOL, 0},
	{"video", JANUS_JSON_BOOL, 0},
	{"peer_audio", JANUS_JSON_BOOL, 0},
	{"peer_video", JANUS_JSON_BOOL, 0},
	{"filename", JSON_STRING, 0}
};
static struct janus_json_parameter keyframe_parameters[] = {
	{"user", JANUS_JSON_BOOL, 0},
	{"peer", JANUS_JSON_BOOL, 0}
};

/* Useful stuff */
static volatile gint initialized = 0, stopping = 0;
static gboolean notify_events = TRUE;
static gboolean ipv6_disabled = FALSE;
static janus_callbacks *gateway = NULL;

static char *local_ip = NULL, *sdp_ip = NULL;
static janus_network_address janus_network_local_ip = { 0 };
#define DEFAULT_RTP_RANGE_MIN 10000
#define DEFAULT_RTP_RANGE_MAX 60000
static uint16_t rtp_range_min = DEFAULT_RTP_RANGE_MIN;
static uint16_t rtp_range_max = DEFAULT_RTP_RANGE_MAX;
static uint16_t rtp_range_slider = DEFAULT_RTP_RANGE_MIN;
static int dscp_audio_rtp = 0;
static int dscp_video_rtp = 0;

static GThread *handler_thread;
static void *janus_nosip_handler(void *data);
static void janus_nosip_hangup_media_internal(janus_plugin_session *handle);

typedef struct janus_nosip_message {
	janus_plugin_session *handle;
	char *transaction;
	json_t *message;
	json_t *jsep;
} janus_nosip_message;
static GAsyncQueue *messages = NULL;
static janus_nosip_message exit_message;


typedef struct janus_nosip_media {
	char *remote_audio_ip;
	char *remote_video_ip;
	gboolean ready;
	gboolean require_srtp, has_srtp_local, has_srtp_remote;
	janus_srtp_profile srtp_profile;
	gboolean has_audio;
	int audio_rtp_fd, audio_rtcp_fd;
	int local_audio_rtp_port, remote_audio_rtp_port;
	int local_audio_rtcp_port, remote_audio_rtcp_port;
	guint32 audio_ssrc, audio_ssrc_peer;
	int audio_pt, opusred_pt;
	const char *audio_pt_name;
	gint32 audio_srtp_tag;
	srtp_t audio_srtp_in, audio_srtp_out;
	srtp_policy_t audio_remote_policy, audio_local_policy;
	char *audio_srtp_local_profile, *audio_srtp_local_crypto;
	gboolean audio_send;
	gboolean has_video;
	int video_rtp_fd, video_rtcp_fd;
	int local_video_rtp_port, remote_video_rtp_port;
	int local_video_rtcp_port, remote_video_rtcp_port;
	guint32 video_ssrc, video_ssrc_peer;
	guint32 simulcast_ssrc;
	int video_pt;
	const char *video_pt_name;
	gint32 video_srtp_tag;
	srtp_t video_srtp_in, video_srtp_out;
	srtp_policy_t video_remote_policy, video_local_policy;
	char *video_srtp_local_profile, *video_srtp_local_crypto;
	gboolean video_send;
	gboolean video_pli_supported;
	janus_rtp_switching_context acontext, vcontext;
	int pipefd[2];
	gboolean updated;
	int video_orientation_extension_id;
	int audio_level_extension_id;
} janus_nosip_media;

typedef struct janus_nosip_session {
	janus_plugin_session *handle;
	gint64 sdp_version;
	janus_nosip_media media;	/* Media gatewaying stuff (same stuff as the SIP plugin) */
	janus_sdp *sdp;				/* The SDP this user sent */
	janus_recorder *arc;		/* The Janus recorder instance for this user's audio, if enabled */
	janus_recorder *arc_peer;	/* The Janus recorder instance for the peer's audio, if enabled */
	janus_recorder *vrc;		/* The Janus recorder instance for this user's video, if enabled */
	janus_recorder *vrc_peer;	/* The Janus recorder instance for the peer's video, if enabled */
	janus_mutex rec_mutex;		/* Mutex to protect the recorders from race conditions */
	GThread *relayer_thread;
	volatile gint hangingup;
	volatile gint destroyed;
	janus_refcount ref;
	janus_mutex mutex;
} janus_nosip_session;
static GHashTable *sessions;
static janus_mutex sessions_mutex = JANUS_MUTEX_INITIALIZER;

static void janus_nosip_srtp_cleanup(janus_nosip_session *session);
static void janus_nosip_media_reset(janus_nosip_session *session);
static void janus_nosip_rtcp_pli_send(janus_nosip_session *session);

static void janus_nosip_session_destroy(janus_nosip_session *session) {
	if(session && g_atomic_int_compare_and_exchange(&session->destroyed, 0, 1))
		janus_refcount_decrease(&session->ref);
}

static void janus_nosip_session_free(const janus_refcount *session_ref) {
	janus_nosip_session *session = janus_refcount_containerof(session_ref, janus_nosip_session, ref);
	/* Remove the reference to the core plugin session */
	janus_refcount_decrease(&session->handle->ref);
	/* This session can be destroyed, free all the resources */
	janus_sdp_destroy(session->sdp);
	session->sdp = NULL;
	g_free(session->media.remote_audio_ip);
	session->media.remote_audio_ip = NULL;
	g_free(session->media.remote_video_ip);
	session->media.remote_video_ip = NULL;
	janus_nosip_srtp_cleanup(session);
	session->handle = NULL;
	janus_mutex_destroy(&session->mutex);
	janus_mutex_destroy(&session->rec_mutex);
	g_free(session);
	session = NULL;
}

static void janus_nosip_message_free(janus_nosip_message *msg) {
	if(!msg || msg == &exit_message)
		return;

	if(msg->handle && msg->handle->plugin_handle) {
		janus_nosip_session *session = (janus_nosip_session *)msg->handle->plugin_handle;
		janus_refcount_decrease(&session->ref);
	}
	msg->handle = NULL;

	g_free(msg->transaction);
	msg->transaction = NULL;
	if(msg->message)
		json_decref(msg->message);
	msg->message = NULL;
	if(msg->jsep)
		json_decref(msg->jsep);
	msg->jsep = NULL;

	g_free(msg);
}


/* SRTP stuff (in case we need SDES) */
static int janus_nosip_srtp_set_local(janus_nosip_session *session, gboolean video, char **profile, char **crypto) {
	if(session == NULL)
		return -1;
	/* Which SRTP profile are we going to negotiate? */
	int key_length = 0, salt_length = 0, master_length = 0;
	if(session->media.srtp_profile == JANUS_SRTP_AES128_CM_SHA1_32) {
		key_length = SRTP_MASTER_KEY_LENGTH;
		salt_length = SRTP_MASTER_SALT_LENGTH;
		master_length = SRTP_MASTER_LENGTH;
		*profile = g_strdup("AES_CM_128_HMAC_SHA1_32");
	} else if(session->media.srtp_profile == JANUS_SRTP_AES128_CM_SHA1_80) {
		key_length = SRTP_MASTER_KEY_LENGTH;
		salt_length = SRTP_MASTER_SALT_LENGTH;
		master_length = SRTP_MASTER_LENGTH;
		*profile = g_strdup("AES_CM_128_HMAC_SHA1_80");
#ifdef HAVE_SRTP_AESGCM
	} else if(session->media.srtp_profile == JANUS_SRTP_AEAD_AES_128_GCM) {
		key_length = SRTP_AESGCM128_MASTER_KEY_LENGTH;
		salt_length = SRTP_AESGCM128_MASTER_SALT_LENGTH;
		master_length = SRTP_AESGCM128_MASTER_LENGTH;
		*profile = g_strdup("AEAD_AES_128_GCM");
	} else if(session->media.srtp_profile == JANUS_SRTP_AEAD_AES_256_GCM) {
		key_length = SRTP_AESGCM256_MASTER_KEY_LENGTH;
		salt_length = SRTP_AESGCM256_MASTER_SALT_LENGTH;
		master_length = SRTP_AESGCM256_MASTER_LENGTH;
		*profile = g_strdup("AEAD_AES_256_GCM");
#endif
	} else {
		JANUS_LOG(LOG_ERR, "[NoSIP-%p] Unsupported SRTP profile\n", session);
		return -2;
	}
	JANUS_LOG(LOG_WARN, "[NoSIP-%p] %s\n", session, *profile);
	JANUS_LOG(LOG_WARN, "[NoSIP-%p] Key/Salt/Master: %d/%d/%d\n",
		session, master_length, key_length, salt_length);
	/* Generate key/salt */
	uint8_t *key = g_malloc0(master_length);
	srtp_crypto_get_random(key, master_length);
	/* Set SRTP policies */
	srtp_policy_t *policy = video ? &session->media.video_local_policy : &session->media.audio_local_policy;
	switch(session->media.srtp_profile) {
		case JANUS_SRTP_AES128_CM_SHA1_32:
			srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(&(policy->rtp));
			srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&(policy->rtcp));
			break;
		case JANUS_SRTP_AES128_CM_SHA1_80:
			srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&(policy->rtp));
			srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&(policy->rtcp));
			break;
#ifdef HAVE_SRTP_AESGCM
		case JANUS_SRTP_AEAD_AES_128_GCM:
			srtp_crypto_policy_set_aes_gcm_128_16_auth(&(policy->rtp));
			srtp_crypto_policy_set_aes_gcm_128_16_auth(&(policy->rtcp));
			break;
		case JANUS_SRTP_AEAD_AES_256_GCM:
			srtp_crypto_policy_set_aes_gcm_256_16_auth(&(policy->rtp));
			srtp_crypto_policy_set_aes_gcm_256_16_auth(&(policy->rtcp));
			break;
#endif
		default:
			/* Will never happen? */
			JANUS_LOG(LOG_WARN, "[NoSIP-%p] Unsupported SRTP profile\n", session);
			break;
	}
	policy->ssrc.type = ssrc_any_inbound;
	policy->key = key;
	policy->next = NULL;
	/* Create SRTP context */
	srtp_err_status_t res = srtp_create(video ? &session->media.video_srtp_out : &session->media.audio_srtp_out, policy);
	if(res != srtp_err_status_ok) {
		/* Something went wrong... */
		JANUS_LOG(LOG_ERR, "Oops, error creating outbound SRTP session: %d (%s)\n", res, janus_srtp_error_str(res));
		g_free(*profile);
		*profile = NULL;
		g_free(key);
		policy->key = NULL;
		return -2;
	}
	/* Base64 encode the salt */
	*crypto = g_base64_encode(key, master_length);
	if((video && session->media.video_srtp_out) || (!video && session->media.audio_srtp_out)) {
		JANUS_LOG(LOG_VERB, "%s outbound SRTP session created\n", video ? "Video" : "Audio");
	}
	return 0;
}
static int janus_nosip_srtp_set_remote(janus_nosip_session *session, gboolean video, const char *profile, const char *crypto) {
	if(session == NULL || profile == NULL || crypto == NULL)
		return -1;
	/* Which SRTP profile is being negotiated? */
	JANUS_LOG(LOG_WARN, "[NoSIP-%p] %s\n", session, profile);
	gsize key_length = 0, salt_length = 0, master_length = 0;
	if(!strcasecmp(profile, "AES_CM_128_HMAC_SHA1_32")) {
		session->media.srtp_profile = JANUS_SRTP_AES128_CM_SHA1_32;
		key_length = SRTP_MASTER_KEY_LENGTH;
		salt_length = SRTP_MASTER_SALT_LENGTH;
		master_length = SRTP_MASTER_LENGTH;
	} else if(!strcasecmp(profile, "AES_CM_128_HMAC_SHA1_80")) {
		session->media.srtp_profile = JANUS_SRTP_AES128_CM_SHA1_80;
		key_length = SRTP_MASTER_KEY_LENGTH;
		salt_length = SRTP_MASTER_SALT_LENGTH;
		master_length = SRTP_MASTER_LENGTH;
#ifdef HAVE_SRTP_AESGCM
	} else if(!strcasecmp(profile, "AEAD_AES_128_GCM")) {
		session->media.srtp_profile = JANUS_SRTP_AEAD_AES_128_GCM;
		key_length = SRTP_AESGCM128_MASTER_KEY_LENGTH;
		salt_length = SRTP_AESGCM128_MASTER_SALT_LENGTH;
		master_length = SRTP_AESGCM128_MASTER_LENGTH;
	} else if(!strcasecmp(profile, "AEAD_AES_256_GCM")) {
		session->media.srtp_profile = JANUS_SRTP_AEAD_AES_256_GCM;
		key_length = SRTP_AESGCM256_MASTER_KEY_LENGTH;
		salt_length = SRTP_AESGCM256_MASTER_SALT_LENGTH;
		master_length = SRTP_AESGCM256_MASTER_LENGTH;
#endif
	} else {
		JANUS_LOG(LOG_WARN, "[NoSIP-%p] Unsupported SRTP profile %s\n", session, profile);
		return -2;
	}
	JANUS_LOG(LOG_VERB, "[NoSIP-%p] Key/Salt/Master: %zu/%zu/%zu\n",
		session, master_length, key_length, salt_length);
	/* Base64 decode the crypto string and set it as the remote SRTP context */
	gsize len = 0;
	guchar *decoded = g_base64_decode(crypto, &len);
	if(len < master_length) {
		/* FIXME Can this happen? */
		g_free(decoded);
		return -3;
	}
	/* Set SRTP policies */
	srtp_policy_t *policy = video ? &session->media.video_remote_policy : &session->media.audio_remote_policy;
	switch(session->media.srtp_profile) {
		case JANUS_SRTP_AES128_CM_SHA1_32:
			srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(&(policy->rtp));
			srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&(policy->rtcp));
			break;
		case JANUS_SRTP_AES128_CM_SHA1_80:
			srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&(policy->rtp));
			srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&(policy->rtcp));
			break;
#ifdef HAVE_SRTP_AESGCM
		case JANUS_SRTP_AEAD_AES_128_GCM:
			srtp_crypto_policy_set_aes_gcm_128_16_auth(&(policy->rtp));
			srtp_crypto_policy_set_aes_gcm_128_16_auth(&(policy->rtcp));
			break;
		case JANUS_SRTP_AEAD_AES_256_GCM:
			srtp_crypto_policy_set_aes_gcm_256_16_auth(&(policy->rtp));
			srtp_crypto_policy_set_aes_gcm_256_16_auth(&(policy->rtcp));
			break;
#endif
		default:
			/* Will never happen? */
			JANUS_LOG(LOG_WARN, "[NoSIP-%p] Unsupported SRTP profile\n", session);
			break;
	}
	policy->ssrc.type = ssrc_any_inbound;
	policy->key = decoded;
	policy->next = NULL;
	/* Create SRTP context */
	srtp_err_status_t res = srtp_create(video ? &session->media.video_srtp_in : &session->media.audio_srtp_in, policy);
	if(res != srtp_err_status_ok) {
		/* Something went wrong... */
		JANUS_LOG(LOG_ERR, "Oops, error creating inbound SRTP session: %d (%s)\n", res, janus_srtp_error_str(res));
		g_free(decoded);
		policy->key = NULL;
		return -2;
	}
	if((video && session->media.video_srtp_in) || (!video && session->media.audio_srtp_in)) {
		JANUS_LOG(LOG_VERB, "%s inbound SRTP session created\n", video ? "Video" : "Audio");
	}
	return 0;
}
static void janus_nosip_srtp_cleanup(janus_nosip_session *session) {
	if(session == NULL)
		return;
	session->media.require_srtp = FALSE;
	session->media.has_srtp_local = FALSE;
	session->media.has_srtp_remote = FALSE;
	session->media.srtp_profile = 0;
	/* Audio */
	session->media.audio_srtp_tag = 0;
	if(session->media.audio_srtp_out)
		srtp_dealloc(session->media.audio_srtp_out);
	session->media.audio_srtp_out = NULL;
	g_free(session->media.audio_local_policy.key);
	session->media.audio_local_policy.key = NULL;
	if(session->media.audio_srtp_in)
		srtp_dealloc(session->media.audio_srtp_in);
	session->media.audio_srtp_in = NULL;
	g_free(session->media.audio_remote_policy.key);
	session->media.audio_remote_policy.key = NULL;
	if(session->media.audio_srtp_local_profile) {
		g_free(session->media.audio_srtp_local_profile);
		session->media.audio_srtp_local_profile = NULL;
	}
	if(session->media.audio_srtp_local_crypto) {
		g_free(session->media.audio_srtp_local_crypto);
		session->media.audio_srtp_local_crypto = NULL;
	}
	/* Video */
	session->media.video_srtp_tag = 0;
	if(session->media.video_srtp_out)
		srtp_dealloc(session->media.video_srtp_out);
	session->media.video_srtp_out = NULL;
	g_free(session->media.video_local_policy.key);
	session->media.video_local_policy.key = NULL;
	if(session->media.video_srtp_in)
		srtp_dealloc(session->media.video_srtp_in);
	session->media.video_srtp_in = NULL;
	g_free(session->media.video_remote_policy.key);
	session->media.video_remote_policy.key = NULL;
	if(session->media.video_srtp_local_profile) {
		g_free(session->media.video_srtp_local_profile);
		session->media.video_srtp_local_profile = NULL;
	}
	if(session->media.video_srtp_local_crypto) {
		g_free(session->media.video_srtp_local_crypto);
		session->media.video_srtp_local_crypto = NULL;
	}
}

void janus_nosip_media_reset(janus_nosip_session *session) {
	if(session == NULL)
		return;
	g_free(session->media.remote_audio_ip);
	session->media.remote_audio_ip = NULL;
	g_free(session->media.remote_video_ip);
	session->media.remote_video_ip = NULL;
	session->media.updated = FALSE;
	session->media.ready = FALSE;
	session->media.require_srtp = FALSE;
	session->media.has_audio = FALSE;
	session->media.audio_pt = -1;
	session->media.opusred_pt = -1;
	session->media.audio_pt_name = NULL;	/* Immutable string, no need to free*/
	session->media.audio_send = TRUE;
	session->media.has_video = FALSE;
	session->media.video_pt = -1;
	session->media.video_pt_name = NULL;	/* Immutable string, no need to free*/
	session->media.video_send = TRUE;
	session->media.video_pli_supported = FALSE;
	session->media.video_orientation_extension_id = -1;
	session->media.audio_level_extension_id = -1;
	janus_rtp_switching_context_reset(&session->media.acontext);
	janus_rtp_switching_context_reset(&session->media.vcontext);
}


/* SDP parsing and manipulation */
void janus_nosip_sdp_process(janus_nosip_session *session, janus_sdp *sdp, gboolean answer, gboolean update, gboolean *changed);
char *janus_nosip_sdp_manipulate(janus_nosip_session *session, janus_sdp *sdp, gboolean answer);
/* Media */
static int janus_nosip_allocate_local_ports(janus_nosip_session *session, gboolean update);
static void *janus_nosip_relay_thread(void *data);
static void janus_nosip_media_cleanup(janus_nosip_session *session);


/* Error codes */
#define JANUS_NOSIP_ERROR_UNKNOWN_ERROR			499
#define JANUS_NOSIP_ERROR_NO_MESSAGE			440
#define JANUS_NOSIP_ERROR_INVALID_JSON			441
#define JANUS_NOSIP_ERROR_INVALID_REQUEST		442
#define JANUS_NOSIP_ERROR_MISSING_ELEMENT		443
#define JANUS_NOSIP_ERROR_INVALID_ELEMENT		444
#define JANUS_NOSIP_ERROR_WRONG_STATE			445
#define JANUS_NOSIP_ERROR_MISSING_SDP			446
#define JANUS_NOSIP_ERROR_INVALID_SDP			447
#define JANUS_NOSIP_ERROR_IO_ERROR				448
#define JANUS_NOSIP_ERROR_RECORDING_ERROR		449
#define JANUS_NOSIP_ERROR_TOO_STRICT			450


/* Plugin implementation */
int janus_nosip_init(janus_callbacks *callback, const char *config_path) {
	if(g_atomic_int_get(&stopping)) {
		/* Still stopping from before */
		return -1;
	}
	if(callback == NULL || config_path == NULL) {
		/* Invalid arguments */
		return -1;
	}

	/* Read configuration */
	char filename[255];
	g_snprintf(filename, 255, "%s/%s.jcfg", config_path, JANUS_NOSIP_PACKAGE);
	JANUS_LOG(LOG_VERB, "Configuration file: %s\n", filename);
	janus_config *config = janus_config_parse(filename);
	if(config == NULL) {
		JANUS_LOG(LOG_WARN, "Couldn't find .jcfg configuration file (%s), trying .cfg\n", JANUS_NOSIP_PACKAGE);
		g_snprintf(filename, 255, "%s/%s.cfg", config_path, JANUS_NOSIP_PACKAGE);
		JANUS_LOG(LOG_VERB, "Configuration file: %s\n", filename);
		config = janus_config_parse(filename);
	}
	if(config != NULL) {
		janus_config_print(config);

		janus_config_category *config_general = janus_config_get_create(config, NULL, janus_config_type_category, "general");
		janus_config_item *item = janus_config_get(config, config_general, janus_config_type_item, "local_ip");
		if(item && item->value && strlen(item->value) > 0) {
			/* Verify that the address is valid */
			struct ifaddrs *ifas = NULL;
			janus_network_address iface;
			janus_network_address_string_buffer ibuf;
			if(getifaddrs(&ifas) == -1) {
				JANUS_LOG(LOG_ERR, "Unable to acquire list of network devices/interfaces; some configurations may not work as expected... %d (%s)\n",
					errno, g_strerror(errno));
			} else {
				if(janus_network_lookup_interface(ifas, item->value, &iface) != 0) {
					JANUS_LOG(LOG_WARN, "Error setting local IP address to %s, falling back to detecting IP address...\n", item->value);
				} else {
					if(janus_network_address_to_string_buffer(&iface, &ibuf) != 0 || janus_network_address_string_buffer_is_null(&ibuf)) {
						JANUS_LOG(LOG_WARN, "Error getting local IP address from %s, falling back to detecting IP address...\n", item->value);
					} else {
						local_ip = g_strdup(janus_network_address_string_from_buffer(&ibuf));
					}
				}
				freeifaddrs(ifas);
			}
		}

		item = janus_config_get(config, config_general, janus_config_type_item, "sdp_ip");
		if(item && item->value && strlen(item->value) > 0) {
			sdp_ip = g_strdup(item->value);
			JANUS_LOG(LOG_VERB, "IP to advertise in SDP: %s\n", sdp_ip);
		}

		/* Make sure both IPs are valid, if provided */
		janus_network_address_nullify(&janus_network_local_ip);
		if(local_ip) {
			if(janus_network_string_to_address(janus_network_query_options_any_ip, local_ip, &janus_network_local_ip) != 0) {
				JANUS_LOG(LOG_ERR, "Invalid local media IP address [%s]...\n", local_ip);
				return -1;
			}
			if((janus_network_local_ip.family == AF_INET && janus_network_local_ip.ipv4.s_addr == INADDR_ANY) ||
					(janus_network_local_ip.family == AF_INET6 && IN6_IS_ADDR_UNSPECIFIED(&janus_network_local_ip.ipv6))) {
				janus_network_address_nullify(&janus_network_local_ip);
			}
		}
		JANUS_LOG(LOG_VERB, "Binding media address set to [%s]...\n", janus_network_address_is_null(&janus_network_local_ip) ? "any" : local_ip);
		if(!sdp_ip) {
			char *ip = janus_network_address_is_null(&janus_network_local_ip) ? local_ip : NULL;
			if(ip) {
				sdp_ip = g_strdup(ip);
				JANUS_LOG(LOG_VERB, "IP to advertise in SDP: %s\n", sdp_ip);
			}
		}

		item = janus_config_get(config, config_general, janus_config_type_item, "rtp_port_range");
		if(item && item->value) {
			/* Split in min and max port */
			char *maxport = strrchr(item->value, '-');
			if(maxport != NULL) {
				*maxport = '\0';
				maxport++;
				if(janus_string_to_uint16(item->value, &rtp_range_min) < 0)
					JANUS_LOG(LOG_WARN, "Invalid RTP min port value: %s (assuming 0)\n", item->value);
				if(janus_string_to_uint16(maxport, &rtp_range_max) < 0)
					JANUS_LOG(LOG_WARN, "Invalid RTP max port value: %s (assuming 0)\n", maxport);
				maxport--;
				*maxport = '-';
			}
			if(rtp_range_min > rtp_range_max) {
				uint16_t temp_port = rtp_range_min;
				rtp_range_min = rtp_range_max;
				rtp_range_max = temp_port;
			}
			if(rtp_range_min % 2)
				rtp_range_min++;	/* Pick an even port for RTP */
			if(rtp_range_min > rtp_range_max) {
				JANUS_LOG(LOG_WARN, "Incorrect port range (%u -- %u), switching min and max\n", rtp_range_min, rtp_range_max);
				uint16_t range_temp = rtp_range_max;
				rtp_range_max = rtp_range_min;
				rtp_range_min = range_temp;
			}
			if(rtp_range_max == 0)
				rtp_range_max = 65535;
			rtp_range_slider = rtp_range_min;
			JANUS_LOG(LOG_VERB, "NoSIP RTP/RTCP port range: %u -- %u\n", rtp_range_min, rtp_range_max);
		}

		item = janus_config_get(config, config_general, janus_config_type_item, "events");
		if(item != NULL && item->value != NULL)
			notify_events = janus_is_true(item->value);
		if(!notify_events && callback->events_is_enabled()) {
			JANUS_LOG(LOG_WARN, "Notification of events to handlers disabled for %s\n", JANUS_NOSIP_NAME);
		}

		/* Is there any DSCP TOS to apply? */
		item = janus_config_get(config, config_general, janus_config_type_item, "dscp_audio_rtp");
		if(item && item->value) {
			int val = atoi(item->value);
			if(val < 0) {
				JANUS_LOG(LOG_WARN, "Ignoring dscp_audio_rtp value as it's not a positive integer\n");
			} else {
				dscp_audio_rtp = val;
			}
		}
		item = janus_config_get(config, config_general, janus_config_type_item, "dscp_video_rtp");
		if(item && item->value) {
			int val = atoi(item->value);
			if(val < 0) {
				JANUS_LOG(LOG_WARN, "Ignoring dscp_video_rtp value as it's not a positive integer\n");
			} else {
				dscp_video_rtp = val;
			}
		}

		janus_config_destroy(config);
	}
	config = NULL;

	if(local_ip == NULL) {
		local_ip = janus_network_detect_local_ip_as_string(janus_network_query_options_any_ip);
		if(local_ip == NULL) {
			JANUS_LOG(LOG_WARN, "Couldn't find any address! using 127.0.0.1 as the local IP... (which is NOT going to work out of your machine)\n");
			local_ip = g_strdup("127.0.0.1");
		}
	}
	JANUS_LOG(LOG_VERB, "Local IP set to %s\n", local_ip);

	sessions = g_hash_table_new_full(NULL, NULL, NULL, (GDestroyNotify)janus_nosip_session_destroy);
	messages = g_async_queue_new_full((GDestroyNotify) janus_nosip_message_free);
	/* This is the callback we'll need to invoke to contact the Janus core */
	gateway = callback;

	if(janus_network_address_is_null(&janus_network_local_ip) ||
			janus_network_local_ip.family == AF_INET6) {
		/* Finally, let's check if IPv6 is disabled, as we may need to know for RTP/RTCP sockets */
		int fd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
		if(fd <= 0) {
			ipv6_disabled = TRUE;
		} else {
			int v6only = 0;
			if(setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only)) != 0)
				ipv6_disabled = TRUE;
		}
		if(fd > 0)
			close(fd);
		if(ipv6_disabled) {
			if(!janus_network_address_is_null(&janus_network_local_ip)) {
				JANUS_LOG(LOG_ERR, "IPv6 disabled and local media address is IPv6...\n");
				return -1;
			}
			JANUS_LOG(LOG_WARN, "IPv6 disabled, will only use IPv4 for RTP/RTCP sockets (SIP)\n");
		}
	} else if(janus_network_local_ip.family == AF_INET) {
		/* Disable if we have a specified IPv4 address for RTP/RTCP sockets */
		ipv6_disabled = TRUE;
	}

	g_atomic_int_set(&initialized, 1);

	GError *error = NULL;
	/* Launch the thread that will handle incoming messages */
	handler_thread = g_thread_try_new("nosip handler", janus_nosip_handler, NULL, &error);
	if(error != NULL) {
		g_atomic_int_set(&initialized, 0);
		JANUS_LOG(LOG_ERR, "Got error %d (%s) trying to launch the NoSIP handler thread...\n",
			error->code, error->message ? error->message : "??");
		g_error_free(error);
		return -1;
	}
	JANUS_LOG(LOG_INFO, "%s initialized!\n", JANUS_NOSIP_NAME);
	return 0;
}

void janus_nosip_destroy(void) {
	if(!g_atomic_int_get(&initialized))
		return;
	g_atomic_int_set(&stopping, 1);

	g_async_queue_push(messages, &exit_message);
	if(handler_thread != NULL) {
		g_thread_join(handler_thread);
		handler_thread = NULL;
	}
	/* FIXME We should destroy the sessions cleanly */
	janus_mutex_lock(&sessions_mutex);
	g_hash_table_destroy(sessions);
	sessions = NULL;
	janus_mutex_unlock(&sessions_mutex);
	g_async_queue_unref(messages);
	messages = NULL;
	g_atomic_int_set(&initialized, 0);
	g_atomic_int_set(&stopping, 0);

	g_free(local_ip);
	g_free(sdp_ip);

	JANUS_LOG(LOG_INFO, "%s destroyed!\n", JANUS_NOSIP_NAME);
}

int janus_nosip_get_api_compatibility(void) {
	/* Important! This is what your plugin MUST always return: don't lie here or bad things will happen */
	return JANUS_PLUGIN_API_VERSION;
}

int janus_nosip_get_version(void) {
	return JANUS_NOSIP_VERSION;
}

const char *janus_nosip_get_version_string(void) {
	return JANUS_NOSIP_VERSION_STRING;
}

const char *janus_nosip_get_description(void) {
	return JANUS_NOSIP_DESCRIPTION;
}

const char *janus_nosip_get_name(void) {
	return JANUS_NOSIP_NAME;
}

const char *janus_nosip_get_author(void) {
	return JANUS_NOSIP_AUTHOR;
}

const char *janus_nosip_get_package(void) {
	return JANUS_NOSIP_PACKAGE;
}

static janus_nosip_session *janus_nosip_lookup_session(janus_plugin_session *handle) {
	janus_nosip_session *session = NULL;
	if (g_hash_table_contains(sessions, handle)) {
		session = (janus_nosip_session *)handle->plugin_handle;
	}
	return session;
}

void janus_nosip_create_session(janus_plugin_session *handle, int *error) {
	if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized)) {
		*error = -1;
		return;
	}
	janus_nosip_session *session = g_malloc0(sizeof(janus_nosip_session));
	session->handle = handle;
	session->sdp = NULL;
	session->media.remote_audio_ip = NULL;
	session->media.remote_video_ip = NULL;
	session->media.ready = FALSE;
	session->media.require_srtp = FALSE;
	session->media.has_srtp_local = FALSE;
	session->media.has_srtp_remote = FALSE;
	session->media.srtp_profile = 0;
	session->media.audio_srtp_local_profile = NULL;
	session->media.audio_srtp_local_crypto = NULL;
	session->media.video_srtp_local_profile = NULL;
	session->media.video_srtp_local_crypto = NULL;
	session->media.has_audio = FALSE;
	session->media.audio_rtp_fd = -1;
	session->media.audio_rtcp_fd = -1;
	session->media.local_audio_rtp_port = 0;
	session->media.remote_audio_rtp_port = 0;
	session->media.local_audio_rtcp_port = 0;
	session->media.remote_audio_rtcp_port = 0;
	session->media.audio_ssrc = 0;
	session->media.audio_ssrc_peer = 0;
	session->media.audio_pt = -1;
	session->media.opusred_pt = -1;
	session->media.audio_pt_name = NULL;
	session->media.audio_send = TRUE;
	session->media.has_video = FALSE;
	session->media.video_rtp_fd = -1;
	session->media.video_rtcp_fd = -1;
	session->media.local_video_rtp_port = 0;
	session->media.remote_video_rtp_port = 0;
	session->media.local_video_rtcp_port = 0;
	session->media.remote_video_rtcp_port = 0;
	session->media.video_ssrc = 0;
	session->media.video_ssrc_peer = 0;
	session->media.simulcast_ssrc = 0;
	session->media.video_pt = -1;
	session->media.video_pt_name = NULL;
	session->media.video_send = TRUE;
	session->media.video_pli_supported = FALSE;
	session->media.video_orientation_extension_id = -1;
	session->media.audio_level_extension_id = -1;
	/* Initialize the RTP context */
	janus_rtp_switching_context_reset(&session->media.acontext);
	janus_rtp_switching_context_reset(&session->media.vcontext);
	session->media.pipefd[0] = -1;
	session->media.pipefd[1] = -1;
	session->media.updated = FALSE;
	session->media.audio_remote_policy.ssrc.type = ssrc_any_inbound;
	session->media.audio_local_policy.ssrc.type = ssrc_any_inbound;
	session->media.video_remote_policy.ssrc.type = ssrc_any_inbound;
	session->media.video_local_policy.ssrc.type = ssrc_any_inbound;
	janus_mutex_init(&session->rec_mutex);
	g_atomic_int_set(&session->destroyed, 0);
	g_atomic_int_set(&session->hangingup, 0);
	janus_mutex_init(&session->mutex);
	handle->plugin_handle = session;
	janus_refcount_init(&session->ref, janus_nosip_session_free);

	janus_mutex_lock(&sessions_mutex);
	g_hash_table_insert(sessions, handle, session);
	janus_mutex_unlock(&sessions_mutex);

	return;
}

void janus_nosip_destroy_session(janus_plugin_session *handle, int *error) {
	if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized)) {
		*error = -1;
		return;
	}
	janus_mutex_lock(&sessions_mutex);
	janus_nosip_session *session = janus_nosip_lookup_session(handle);
	if(!session) {
		janus_mutex_unlock(&sessions_mutex);
		JANUS_LOG(LOG_ERR, "No NoSIP session associated with this handle...\n");
		*error = -2;
		return;
	}
	JANUS_LOG(LOG_VERB, "Destroying NoSIP session (%p)...\n", session);
	janus_nosip_hangup_media_internal(handle);
	g_hash_table_remove(sessions, handle);
	janus_mutex_unlock(&sessions_mutex);
	return;
}

json_t *janus_nosip_query_session(janus_plugin_session *handle) {
	if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized)) {
		return NULL;
	}
	janus_mutex_lock(&sessions_mutex);
	janus_nosip_session *session = janus_nosip_lookup_session(handle);
	if(!session) {
		janus_mutex_unlock(&sessions_mutex);
		JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
		return NULL;
	}
	janus_refcount_increase(&session->ref);
	janus_mutex_unlock(&sessions_mutex);
	/* Provide some generic info, e.g., if we're in a call and with whom */
	json_t *info = json_object();
	if(session->sdp) {
		json_object_set_new(info, "srtp-required", json_string(session->media.require_srtp ? "yes" : "no"));
		json_object_set_new(info, "sdes-local", json_string(session->media.has_srtp_local ? "yes" : "no"));
		json_object_set_new(info, "sdes-remote", json_string(session->media.has_srtp_remote ? "yes" : "no"));
	}
	if(session->arc || session->vrc || session->arc_peer || session->vrc_peer) {
		json_t *recording = json_object();
		if(session->arc && session->arc->filename)
			json_object_set_new(recording, "audio", json_string(session->arc->filename));
		if(session->vrc && session->vrc->filename)
			json_object_set_new(recording, "video", json_string(session->vrc->filename));
		if(session->arc_peer && session->arc_peer->filename)
			json_object_set_new(recording, "audio-peer", json_string(session->arc_peer->filename));
		if(session->vrc_peer && session->vrc_peer->filename)
			json_object_set_new(recording, "video-peer", json_string(session->vrc_peer->filename));
		json_object_set_new(info, "recording", recording);
	}
	json_object_set_new(info, "hangingup", json_integer(g_atomic_int_get(&session->hangingup)));
	json_object_set_new(info, "destroyed", json_integer(g_atomic_int_get(&session->destroyed)));
	janus_refcount_decrease(&session->ref);
	return info;
}

struct janus_plugin_result *janus_nosip_handle_message(janus_plugin_session *handle, char *transaction, json_t *message, json_t *jsep) {
	if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
		return janus_plugin_result_new(JANUS_PLUGIN_ERROR, g_atomic_int_get(&stopping) ? "Shutting down" : "Plugin not initialized", NULL);

	janus_mutex_lock(&sessions_mutex);
	janus_nosip_session *session = janus_nosip_lookup_session(handle);
	if(!session) {
		janus_mutex_unlock(&sessions_mutex);
		return janus_plugin_result_new(JANUS_PLUGIN_ERROR, "No session associated with this handle", NULL);
	}

	/* Increase the reference counter for this session: we'll decrease it after we handle the message */
	janus_refcount_increase(&session->ref);
	janus_mutex_unlock(&sessions_mutex);

	janus_nosip_message *msg = g_malloc(sizeof(janus_nosip_message));
	msg->handle = handle;
	msg->transaction = transaction;
	msg->message = message;
	msg->jsep = jsep;
	g_async_queue_push(messages, msg);

	/* All the requests to this plugin are handled asynchronously */
	return janus_plugin_result_new(JANUS_PLUGIN_OK_WAIT, NULL, NULL);
}

void janus_nosip_setup_media(janus_plugin_session *handle) {
	JANUS_LOG(LOG_INFO, "WebRTC media is now available\n");
	if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
		return;
	janus_mutex_lock(&sessions_mutex);
	janus_nosip_session *session = janus_nosip_lookup_session(handle);
	if(!session) {
		janus_mutex_unlock(&sessions_mutex);
		JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
		return;
	}
	if(g_atomic_int_get(&session->destroyed)) {
		janus_mutex_unlock(&sessions_mutex);
		return;
	}
	g_atomic_int_set(&session->hangingup, 0);
	janus_mutex_unlock(&sessions_mutex);
}

void janus_nosip_incoming_rtp(janus_plugin_session *handle, janus_plugin_rtp *packet) {
	if(handle == NULL || handle->stopped || g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
		return;
	if(gateway) {
		/* Honour the audio/video active flags */
		janus_nosip_session *session = (janus_nosip_session *)handle->plugin_handle;
		if(!session || g_atomic_int_get(&session->destroyed)) {
			JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
			return;
		}
		gboolean video = packet->video;
		char *buf = packet->buffer;
		uint16_t len = packet->length;
		/* Forward to our NoSIP peer */
		if((video && !session->media.video_send) || (!video && !session->media.audio_send)) {
			/* Dropping packet, peer doesn't want to receive it */
			return;
		}
		if(video && session->media.simulcast_ssrc) {
			/* The user is simulcasting: drop everything except the base layer */
			janus_rtp_header *header = (janus_rtp_header *)buf;
			uint32_t ssrc = ntohl(header->ssrc);
			if(ssrc != session->media.simulcast_ssrc) {
				JANUS_LOG(LOG_DBG, "Dropping packet (not base simulcast substream)\n");
				return;
			}
		}
		if((video && session->media.video_ssrc == 0) || (!video && session->media.audio_ssrc == 0)) {
			rtp_header *header = (rtp_header *)buf;
			if(video) {
				session->media.video_ssrc = ntohl(header->ssrc);
			} else {
				session->media.audio_ssrc = ntohl(header->ssrc);
			}
			JANUS_LOG(LOG_VERB, "Got NoSIP %s SSRC: %"SCNu32"\n",
				video ? "video" : "audio",
				video ? session->media.video_ssrc : session->media.audio_ssrc);
		}
		if((video && session->media.has_video && session->media.video_rtp_fd != -1) ||
				(!video && session->media.has_audio && session->media.audio_rtp_fd != -1)) {
			/* Save the frame if we're recording */
			janus_recorder_save_frame(video ? session->vrc : session->arc, buf, len);
			/* Is SRTP involved? */
			if(session->media.has_srtp_local) {
				char sbuf[2048];
				memcpy(&sbuf, buf, len);
				int protected = len;
				int res = srtp_protect(
					(video ? session->media.video_srtp_out : session->media.audio_srtp_out),
					&sbuf, &protected);
				if(res != srtp_err_status_ok) {
					rtp_header *header = (rtp_header *)&sbuf;
					guint32 timestamp = ntohl(header->timestamp);
					guint16 seq = ntohs(header->seq_number);
					JANUS_LOG(LOG_ERR, "[NoSIP-%p] %s SRTP protect error... %s (len=%d-->%d, ts=%"SCNu32", seq=%"SCNu16")...\n",
						session, video ? "Video" : "Audio", janus_srtp_error_str(res), len, protected, timestamp, seq);
				} else {
					/* Forward the frame to the peer */
					if(send((video ? session->media.video_rtp_fd : session->media.audio_rtp_fd), sbuf, protected, 0) < 0) {
						rtp_header *header = (rtp_header *)&sbuf;
						guint32 timestamp = ntohl(header->timestamp);
						guint16 seq = ntohs(header->seq_number);
						JANUS_LOG(LOG_HUGE, "[NoSIP-%p] Error sending %s SRTP packet... %s (len=%d, ts=%"SCNu32", seq=%"SCNu16")...\n",
							session, video ? "Video" : "Audio", g_strerror(errno), protected, timestamp, seq);
					}
				}
			} else {
				/* Forward the frame to the peer */
				if(send((video ? session->media.video_rtp_fd : session->media.audio_rtp_fd), buf, len, 0) < 0) {
					rtp_header *header = (rtp_header *)&buf;
					guint32 timestamp = ntohl(header->timestamp);
					guint16 seq = ntohs(header->seq_number);
					JANUS_LOG(LOG_HUGE, "[NoSIP-%p] Error sending %s RTP packet... %s (len=%d, ts=%"SCNu32", seq=%"SCNu16")...\n",
						session, video ? "Video" : "Audio", g_strerror(errno), len, timestamp, seq);
				}
			}
		}
	}
}

void janus_nosip_incoming_rtcp(janus_plugin_session *handle, janus_plugin_rtcp *packet) {
	if(handle == NULL || handle->stopped || g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
		return;
	if(gateway) {
		janus_nosip_session *session = (janus_nosip_session *)handle->plugin_handle;
		if(!session || g_atomic_int_get(&session->destroyed)) {
			JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
			return;
		}
		gboolean video = packet->video;
		char *buf = packet->buffer;
		uint16_t len = packet->length;
		/* Forward to our NoSIP peer */
		if((video && session->media.has_video && session->media.video_rtcp_fd != -1) ||
				(!video && session->media.has_audio && session->media.audio_rtcp_fd != -1)) {
			/* Fix SSRCs as the Janus core does */
			JANUS_LOG(LOG_HUGE, "[NoSIP-%p] Fixing %s SSRCs (local %u, peer %u)\n",
				session, video ? "video" : "audio",
				(video ? session->media.video_ssrc : session->media.audio_ssrc),
				(video ? session->media.video_ssrc_peer : session->media.audio_ssrc_peer));
			janus_rtcp_fix_ssrc(NULL, (char *)buf, len, video,
				(video ? session->media.video_ssrc : session->media.audio_ssrc),
				(video ? session->media.video_ssrc_peer : session->media.audio_ssrc_peer));
			/* Is SRTP involved? */
			if(session->media.has_srtp_local) {
				char sbuf[2048];
				memcpy(&sbuf, buf, len);
				int protected = len;
				int res = srtp_protect_rtcp(
					(video ? session->media.video_srtp_out : session->media.audio_srtp_out),
					&sbuf, &protected);
				if(res != srtp_err_status_ok) {
					JANUS_LOG(LOG_ERR, "[NoSIP-%p] %s SRTCP protect error... %s (len=%d-->%d)...\n",
						session, video ? "Video" : "Audio",
						janus_srtp_error_str(res), len, protected);
				} else {
					/* Forward the message to the peer */
					if(send((video ? session->media.video_rtcp_fd : session->media.audio_rtcp_fd), sbuf, protected, 0) < 0) {
						JANUS_LOG(LOG_HUGE, "[NoSIP-%p] Error sending SRTCP %s packet... %s (len=%d)...\n",
							session, video ? "Video" : "Audio", g_strerror(errno), protected);
					}
				}
			} else {
				/* Forward the message to the peer */
				if(send((video ? session->media.video_rtcp_fd : session->media.audio_rtcp_fd), buf, len, 0) < 0) {
					JANUS_LOG(LOG_HUGE, "[NoSIP-%p] Error sending RTCP %s packet... %s (len=%d)...\n",
						session, video ? "Video" : "Audio", g_strerror(errno), len);
				}
			}
		}
	}
}

static void janus_nosip_recorder_close(janus_nosip_session *session,
		gboolean stop_audio, gboolean stop_audio_peer, gboolean stop_video, gboolean stop_video_peer) {
	if(session->arc && stop_audio) {
		janus_recorder *rc = session->arc;
		session->arc = NULL;
		janus_recorder_close(rc);
		JANUS_LOG(LOG_INFO, "Closed user's audio recording %s\n", rc->filename ? rc->filename : "??");
		janus_recorder_destroy(rc);
	}
	if(session->arc_peer && stop_audio_peer) {
		janus_recorder *rc = session->arc_peer;
		session->arc_peer = NULL;
		janus_recorder_close(rc);
		JANUS_LOG(LOG_INFO, "Closed peer's audio recording %s\n", rc->filename ? rc->filename : "??");
		janus_recorder_destroy(rc);
	}
	if(session->vrc && stop_video) {
		janus_recorder *rc = session->vrc;
		session->vrc = NULL;
		janus_recorder_close(rc);
		JANUS_LOG(LOG_INFO, "Closed user's video recording %s\n", rc->filename ? rc->filename : "??");
		janus_recorder_destroy(rc);
	}
	if(session->vrc_peer && stop_video_peer) {
		janus_recorder *rc = session->vrc_peer;
		session->vrc_peer = NULL;
		janus_recorder_close(rc);
		JANUS_LOG(LOG_INFO, "Closed peer's video recording %s\n", rc->filename ? rc->filename : "??");
		janus_recorder_destroy(rc);
	}
}

void janus_nosip_hangup_media(janus_plugin_session *handle) {
	janus_mutex_lock(&sessions_mutex);
	janus_nosip_hangup_media_internal(handle);
	janus_mutex_unlock(&sessions_mutex);
}

static void janus_nosip_hangup_media_internal(janus_plugin_session *handle) {
	JANUS_LOG(LOG_INFO, "No WebRTC media anymore\n");
	if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
		return;
	janus_nosip_session *session = janus_nosip_lookup_session(handle);
	if(!session) {
		JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
		return;
	}
	if(g_atomic_int_get(&session->destroyed))
		return;
	if(!g_atomic_int_compare_and_exchange(&session->hangingup, 0, 1))
		return;
	session->media.simulcast_ssrc = 0;
	/* Notify the thread that it's time to go */
	if(session->media.pipefd[1] > 0) {
		int code = 1;
		ssize_t res = 0;
		do {
			res = write(session->media.pipefd[1], &code, sizeof(int));
		} while(res == -1 && errno == EINTR);
	}
	/* Do cleanup if media thread has not been created */
	if(!session->media.ready && !session->relayer_thread) {
		janus_mutex_lock(&session->mutex);
		janus_nosip_media_cleanup(session);
		janus_mutex_unlock(&session->mutex);
	}
	/* Get rid of the recorders, if available */
	janus_mutex_lock(&session->rec_mutex);
	janus_nosip_recorder_close(session, TRUE, TRUE, TRUE, TRUE);
	janus_mutex_unlock(&session->rec_mutex);
	g_atomic_int_set(&session->hangingup, 0);
}

/* Thread to handle incoming messages */
static void *janus_nosip_handler(void *data) {
	JANUS_LOG(LOG_VERB, "Joining NoSIP handler thread\n");
	janus_nosip_message *msg = NULL;
	int error_code = 0;
	char error_cause[512];
	json_t *root = NULL;
	while(g_atomic_int_get(&initialized) && !g_atomic_int_get(&stopping)) {
		msg = g_async_queue_pop(messages);
		if(msg == &exit_message)
			break;
		if(msg->handle == NULL) {
			janus_nosip_message_free(msg);
			continue;
		}
		janus_mutex_lock(&sessions_mutex);
		janus_nosip_session *session = janus_nosip_lookup_session(msg->handle);
		if(!session) {
			janus_mutex_unlock(&sessions_mutex);
			JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
			janus_nosip_message_free(msg);
			continue;
		}
		if(g_atomic_int_get(&session->destroyed)) {
			janus_mutex_unlock(&sessions_mutex);
			janus_nosip_message_free(msg);
			continue;
		}
		janus_mutex_unlock(&sessions_mutex);
		/* Handle request */
		error_code = 0;
		root = msg->message;
		if(msg->message == NULL) {
			JANUS_LOG(LOG_ERR, "No message??\n");
			error_code = JANUS_NOSIP_ERROR_NO_MESSAGE;
			g_snprintf(error_cause, 512, "%s", "No message??");
			goto error;
		}
		if(!json_is_object(root)) {
			JANUS_LOG(LOG_ERR, "JSON error: not an object\n");
			error_code = JANUS_NOSIP_ERROR_INVALID_JSON;
			g_snprintf(error_cause, 512, "JSON error: not an object");
			goto error;
		}
		JANUS_VALIDATE_JSON_OBJECT(root, request_parameters,
			error_code, error_cause, TRUE,
			JANUS_NOSIP_ERROR_MISSING_ELEMENT, JANUS_NOSIP_ERROR_INVALID_ELEMENT);
		if(error_code != 0)
			goto error;
		json_t *request = json_object_get(root, "request");
		const char *request_text = json_string_value(request);
		json_t *result = NULL, *localjsep = NULL;

		if(!strcasecmp(request_text, "generate") || !strcasecmp(request_text, "process")) {
			/* Shared code for two different requests:
			 * 		generate: Take a JSEP offer or answer and generate a barebone SDP the application can use
			 * 		process: Process a remote barebone SDP, and match it to the one we may have generated before */
			gboolean generate = !strcasecmp(request_text, "generate") ? TRUE : FALSE;
			if(generate) {
				JANUS_VALIDATE_JSON_OBJECT(root, generate_parameters,
					error_code, error_cause, TRUE,
					JANUS_NOSIP_ERROR_MISSING_ELEMENT, JANUS_NOSIP_ERROR_INVALID_ELEMENT);
			} else {
				JANUS_VALIDATE_JSON_OBJECT(root, process_parameters,
					error_code, error_cause, TRUE,
					JANUS_NOSIP_ERROR_MISSING_ELEMENT, JANUS_NOSIP_ERROR_INVALID_ELEMENT);
			}
			if(error_code != 0)
				goto error;
			/* Any SDP to handle? if not, something's wrong */
			const char *msg_sdp_type = json_string_value(json_object_get(generate ? msg->jsep : root, "type"));
			const char *msg_sdp = json_string_value(json_object_get(generate ? msg->jsep : root, "sdp"));
			gboolean sdp_update = json_is_true(json_object_get(generate ? msg->jsep : root, "update"));
			if(!generate && session->media.ready) {
				sdp_update = TRUE;
			}
			if(!msg_sdp) {
				JANUS_LOG(LOG_ERR, "Missing SDP\n");
				error_code = JANUS_NOSIP_ERROR_MISSING_SDP;
				g_snprintf(error_cause, 512, "Missing SDP");
				goto error;
			}
			if(!msg_sdp_type || (strcasecmp(msg_sdp_type, "offer") && strcasecmp(msg_sdp_type, "answer"))) {
				JANUS_LOG(LOG_ERR, "Missing or invalid SDP type\n");
				error_code = JANUS_NOSIP_ERROR_MISSING_SDP;
				g_snprintf(error_cause, 512, "Missing or invalid SDP type");
				goto error;
			}
			gboolean offer = !strcasecmp(msg_sdp_type, "offer");
			if(strstr(msg_sdp, "m=application")) {
				JANUS_LOG(LOG_ERR, "The NoSIP plugin does not support DataChannels\n");
				error_code = JANUS_NOSIP_ERROR_MISSING_SDP;
				g_snprintf(error_cause, 512, "The NoSIP plugin does not support DataChannels");
				goto error;
			}
			if(json_is_true(json_object_get(msg->jsep, "e2ee"))) {
				/* Media is encrypted, but legacy endpoints will need unencrypted media frames */
				JANUS_LOG(LOG_ERR, "Media encryption unsupported by this plugin\n");
				error_code = JANUS_NOSIP_ERROR_INVALID_ELEMENT;
				g_snprintf(error_cause, 512, "Media encryption unsupported by this plugin");
				goto error;
			}
			/* Check if the user provided an info string to provide context */
			const char *info = json_string_value(json_object_get(root, "info"));
			/* SDES-SRTP is disabled by default, let's see if we need to enable it */
			gboolean do_srtp = FALSE, require_srtp = FALSE;
			json_t *srtp = json_object_get(root, "srtp");
			if(srtp) {
				const char *srtp_text = json_string_value(srtp);
				if(!strcasecmp(srtp_text, "sdes_optional")) {
					/* Negotiate SDES, but make it optional */
					do_srtp = TRUE;
				} else if(!strcasecmp(srtp_text, "sdes_mandatory")) {
					/* Negotiate SDES, and require it */
					do_srtp = TRUE;
					require_srtp = TRUE;
				} else {
					JANUS_LOG(LOG_ERR, "Invalid element (srtp can only be sdes_optional or sdes_mandatory)\n");
					error_code = JANUS_NOSIP_ERROR_INVALID_ELEMENT;
					g_snprintf(error_cause, 512, "Invalid element (srtp can only be sdes_optional or sdes_mandatory)");
					goto error;
				}
			}
			if(offer && !sdp_update) {
				/* Clean up SRTP stuff from before first, in case it's still needed */
				janus_nosip_srtp_cleanup(session);
				if(do_srtp) {
					JANUS_LOG(LOG_VERB, "Going to negotiate SDES-SRTP (%s)...\n", require_srtp ? "mandatory" : "optional");
				}
			}
			session->media.require_srtp = require_srtp;
			if(generate) {
				if(!offer) {
					do_srtp = do_srtp || session->media.has_srtp_remote;
					/* Make sure the request is consistent with the state (original offer) */
					if(session->media.require_srtp && !session->media.has_srtp_remote) {
						JANUS_LOG(LOG_ERR, "Can't generate answer: SDES-SRTP required, but caller didn't offer it\n");
						error_code = JANUS_NOSIP_ERROR_TOO_STRICT;
						g_snprintf(error_cause, 512, "Can't generate answer: SDES-SRTP required, but caller didn't offer it");
						goto error;
					}
				}
				session->media.has_srtp_local = do_srtp;
				if(do_srtp) {
					/* Any SRTP profile different from the default? */
					janus_srtp_profile srtp_profile = JANUS_SRTP_AES128_CM_SHA1_80;
					const char *profile = json_string_value(json_object_get(root, "srtp_profile"));
					if(profile) {
						if(!strcmp(profile, "AES_CM_128_HMAC_SHA1_32")) {
							srtp_profile = JANUS_SRTP_AES128_CM_SHA1_32;
						} else if(!strcmp(profile, "AES_CM_128_HMAC_SHA1_80")) {
							srtp_profile = JANUS_SRTP_AES128_CM_SHA1_80;
#ifdef HAVE_SRTP_AESGCM
							} else if(!strcmp(profile, "AEAD_AES_128_GCM")) {
								srtp_profile = JANUS_SRTP_AEAD_AES_128_GCM;
							} else if(!strcmp(profile, "AEAD_AES_256_GCM")) {
								srtp_profile = JANUS_SRTP_AEAD_AES_256_GCM;
#endif
						} else {
							JANUS_LOG(LOG_ERR, "Invalid element (unsupported SRTP profile)\n");
							error_code = JANUS_NOSIP_ERROR_INVALID_ELEMENT;
							g_snprintf(error_cause, 512, "Invalid element (unsupported SRTP profile)");
							goto error;
						}
					}
					session->media.srtp_profile = srtp_profile;
				}
			}
			/* Get video-orientation extension id from SDP we got */
			session->media.video_orientation_extension_id = janus_rtp_header_extension_get_id(msg_sdp, JANUS_RTP_EXTMAP_VIDEO_ORIENTATION);
			/* Get audio-level extension id from SDP we got */
			session->media.audio_level_extension_id = janus_rtp_header_extension_get_id(msg_sdp, JANUS_RTP_EXTMAP_AUDIO_LEVEL);
			/* Parse the SDP we got, manipulate some things, and generate a new one */
			char sdperror[100];
			janus_sdp *parsed_sdp = janus_sdp_parse(msg_sdp, sdperror, sizeof(sdperror));
			if(!parsed_sdp) {
				JANUS_LOG(LOG_ERR, "Error parsing SDP: %s\n", sdperror);
				error_code = JANUS_NOSIP_ERROR_MISSING_SDP;
				g_snprintf(error_cause, 512, "Error parsing SDP: %s", sdperror);
				goto error;
			}
			if(generate) {
				/* Allocate RTP ports and merge them with the anonymized SDP */
				if(strstr(msg_sdp, "m=audio") && !strstr(msg_sdp, "m=audio 0")) {
					JANUS_LOG(LOG_VERB, "Going to negotiate audio...\n");
					session->media.has_audio = TRUE;	/* FIXME Maybe we need a better way to signal this */
				}
				if(strstr(msg_sdp, "m=video") && !strstr(msg_sdp, "m=video 0")) {
					JANUS_LOG(LOG_VERB, "Going to negotiate video...\n");
					session->media.has_video = TRUE;	/* FIXME Maybe we need a better way to signal this */
				}
				janus_mutex_lock(&session->mutex);
				if(janus_nosip_allocate_local_ports(session, sdp_update) < 0) {
					janus_mutex_unlock(&session->mutex);
					JANUS_LOG(LOG_ERR, "Could not allocate RTP/RTCP ports\n");
					janus_sdp_destroy(parsed_sdp);
					error_code = JANUS_NOSIP_ERROR_IO_ERROR;
					g_snprintf(error_cause, 512, "Could not allocate RTP/RTCP ports");
					goto error;
				}
				janus_mutex_unlock(&session->mutex);

				char *sdp = janus_nosip_sdp_manipulate(session, parsed_sdp, FALSE);
				if(sdp == NULL) {
					JANUS_LOG(LOG_ERR, "Could not allocate RTP/RTCP ports\n");
					janus_sdp_destroy(parsed_sdp);
					error_code = JANUS_NOSIP_ERROR_IO_ERROR;
					g_snprintf(error_cause, 512, "Could not allocate RTP/RTCP ports");
					goto error;
				}
				/* Take note of the SDP (may be useful for UPDATEs or re-INVITEs) */
				janus_sdp_destroy(session->sdp);
				session->sdp = parsed_sdp;
				JANUS_LOG(LOG_VERB, "Prepared SDP %s for (%p)\n%s", msg_sdp_type, info, sdp);
				g_atomic_int_set(&session->hangingup, 0);
				/* Also notify event handlers */
				if(!sdp_update && notify_events && gateway->events_is_enabled()) {
					json_t *info = json_object();
					json_object_set_new(info, "event", json_string("generated"));
					json_object_set_new(info, "type", json_string(offer ? "offer" : "answer"));
					json_object_set_new(info, "sdp", json_string(sdp));
					gateway->notify_event(&janus_nosip_plugin, session->handle, info);
				}
				/* If the user negotiated simulcasting, just stick with the base substream */
				json_t *msg_simulcast = json_object_get(msg->jsep, "simulcast");
				if(msg_simulcast && json_array_size(msg_simulcast) > 0) {
					JANUS_LOG(LOG_WARN, "Client negotiated simulcasting which we don't do here, falling back to base substream...\n");
					size_t i = 0;
					for(i=0; i<json_array_size(msg_simulcast); i++) {
						json_t *sobj = json_array_get(msg_simulcast, i);
						json_t *s = json_object_get(sobj, "ssrcs");
						if(s && json_array_size(s) > 0)
							session->media.simulcast_ssrc = json_integer_value(json_array_get(s, 0));
						session->media.simulcast_ssrc = json_integer_value(json_object_get(s, "ssrc-0"));
						/* FIXME We're stopping at the first item, there may be more */
						break;
					}
				}
				/* Send the barebone SDP back */
				result = json_object();
				json_object_set_new(result, "event", json_string("generated"));
				json_object_set_new(result, "type", json_string(offer ? "offer" : "answer"));
				json_object_set_new(result, "sdp", json_string(sdp));
				if(sdp_update)
					json_object_set_new(result, "update", json_true());
				g_free(sdp);
			} else {
				/* We got a barebone offer or answer from our peer: process it accordingly */
				gboolean changed = FALSE;
				janus_nosip_sdp_process(session, parsed_sdp, !offer, sdp_update, &changed);
				/* Check if offer has neither audio nor video, fail */
				if(!session->media.has_audio && !session->media.has_video) {
					JANUS_LOG(LOG_ERR, "No audio and no video being negotiated\n");
					janus_sdp_destroy(parsed_sdp);
					error_code = JANUS_NOSIP_ERROR_INVALID_SDP;
					g_snprintf(error_cause, 512, "No audio and no video being negotiated");
					goto error;
				}
				/* Also fail if there's no remote IP address that can be used for RTP */
				if(!session->media.remote_audio_ip && !session->media.remote_video_ip) {
					JANUS_LOG(LOG_ERR, "No remote IP addresses\n");
					janus_sdp_destroy(parsed_sdp);
					error_code = JANUS_NOSIP_ERROR_INVALID_SDP;
					g_snprintf(error_cause, 512, "No remote IP addresses");
					goto error;
				}
				if(session->media.require_srtp && !session->media.has_srtp_remote) {
					JANUS_LOG(LOG_ERR, "Can't process request: SDES-SRTP required, but caller didn't offer it\n");
					error_code = JANUS_NOSIP_ERROR_TOO_STRICT;
					g_snprintf(error_cause, 512, "Can't process request: SDES-SRTP required, but caller didn't offer it");
					goto error;
				}
				/* Take note of the SDP (may be useful for UPDATEs or re-INVITEs) */
				janus_sdp_destroy(session->sdp);
				session->sdp = parsed_sdp;
				/* Also notify event handlers */
				if(!sdp_update && notify_events && gateway->events_is_enabled()) {
					json_t *info = json_object();
					json_object_set_new(info, "event", json_string("processed"));
					json_object_set_new(info, "type", json_string(offer ? "offer" : "answer"));
					json_object_set_new(info, "sdp", json_string(msg_sdp));
					gateway->notify_event(&janus_nosip_plugin, session->handle, info);
				}
				/* Send SDP to the browser */
				result = json_object();
				json_object_set_new(result, "event", json_string("processed"));
				if(session->media.has_srtp_remote) {
					json_object_set_new(result, "srtp",
						json_string(session->media.require_srtp ? "sdes_mandatory" : "sdes_optional"));
				}
				if(sdp_update)
					json_object_set_new(result, "update", json_true());
				localjsep = json_pack("{ssss}", "type", msg_sdp_type, "sdp", msg_sdp);
			}
			/* If this is an answer, start the media */
			if(!sdp_update && !offer) {
				/* Start the media */
				session->media.ready = TRUE;	/* FIXME Maybe we need a better way to signal this */
				GError *error = NULL;
				char tname[16];
				g_snprintf(tname, sizeof(tname), "nosiprtp %p", session);
				janus_refcount_increase(&session->ref);
				session->relayer_thread = g_thread_try_new(tname, janus_nosip_relay_thread, session, &error);
				if(error != NULL) {
					session->relayer_thread = NULL;
					session->media.ready = FALSE;
					janus_refcount_decrease(&session->ref);
					JANUS_LOG(LOG_ERR, "Got error %d (%s) trying to launch the RTP/RTCP thread...\n",
						error->code, error->message ? error->message : "??");
					g_error_free(error);
				}
			}
		} else if(!strcasecmp(request_text, "hangup")) {
			/* Get rid of an ongoing session */
			gateway->close_pc(session->handle);
			result = json_object();
			json_object_set_new(result, "event", json_string("hangingup"));
		} else if(!strcasecmp(request_text, "recording")) {
			/* Start or stop recording */
			JANUS_VALIDATE_JSON_OBJECT(root, recording_parameters,
				error_code, error_cause, TRUE,
				JANUS_NOSIP_ERROR_MISSING_ELEMENT, JANUS_NOSIP_ERROR_INVALID_ELEMENT);
			if(error_code != 0)
				goto error;
			json_t *action = json_object_get(root, "action");
			const char *action_text = json_string_value(action);
			if(strcasecmp(action_text, "start") && strcasecmp(action_text, "stop")) {
				JANUS_LOG(LOG_ERR, "Invalid action (should be start|stop)\n");
				error_code = JANUS_NOSIP_ERROR_INVALID_ELEMENT;
				g_snprintf(error_cause, 512, "Invalid action (should be start|stop)");
				goto error;
			}
			gboolean record_audio = FALSE, record_video = FALSE,	/* No media is recorded by default */
				record_peer_audio = FALSE, record_peer_video = FALSE;
			json_t *audio = json_object_get(root, "audio");
			record_audio = audio ? json_is_true(audio) : FALSE;
			json_t *video = json_object_get(root, "video");
			record_video = video ? json_is_true(video) : FALSE;
			json_t *peer_audio = json_object_get(root, "peer_audio");
			record_peer_audio = peer_audio ? json_is_true(peer_audio) : FALSE;
			json_t *peer_video = json_object_get(root, "peer_video");
			record_peer_video = peer_video ? json_is_true(peer_video) : FALSE;
			if(!record_audio && !record_video && !record_peer_audio && !record_peer_video) {
				JANUS_LOG(LOG_ERR, "Invalid request (at least one of audio, video, peer_audio and peer_video should be true)\n");
				error_code = JANUS_NOSIP_ERROR_RECORDING_ERROR;
				g_snprintf(error_cause, 512, "Invalid request (at least one of audio, video, peer_audio and peer_video should be true)");
				goto error;
			}
			json_t *recfile = json_object_get(root, "filename");
			const char *recording_base = json_string_value(recfile);
			janus_mutex_lock(&session->rec_mutex);
			if(!strcasecmp(action_text, "start")) {
				/* Start recording something */
				janus_recorder *rc = NULL;
				char filename[255];
				gint64 now = janus_get_real_time();
				if(record_peer_audio || record_peer_video) {
					JANUS_LOG(LOG_INFO, "Starting recording of peer's %s\n",
						(record_peer_audio && record_peer_video ? "audio and video" : (record_peer_audio ? "audio" : "video")));
					/* Start recording this peer's audio and/or video */
					if(record_peer_audio) {
						memset(filename, 0, 255);
						if(recording_base) {
							/* Use the filename and path we have been provided */
							g_snprintf(filename, 255, "%s-peer-audio", recording_base);
							/* FIXME This only works if offer/answer happened */
							rc = janus_recorder_create(NULL, session->media.audio_pt_name, filename);
						} else {
							/* Build a filename */
							g_snprintf(filename, 255, "nosip-%p-%"SCNi64"-peer-audio", session, now);
							/* FIXME This only works if offer/answer happened */
							rc = janus_recorder_create(NULL, session->media.audio_pt_name, filename);
						}
						if(rc == NULL) {
							/* FIXME We should notify the fact the recorder could not be created */
							JANUS_LOG(LOG_ERR, "Couldn't open an audio recording file for this peer!\n");
						} else {
							/* If RED is in use, take note of it */
							if(session->media.opusred_pt > 0)
								janus_recorder_opusred(rc, session->media.opusred_pt);
							session->arc_peer = rc;
						}
					}
					if(record_peer_video) {
						memset(filename, 0, 255);
						if(recording_base) {
							/* Use the filename and path we have been provided */
							g_snprintf(filename, 255, "%s-peer-video", recording_base);
							/* FIXME This only works if offer/answer happened */
							rc = janus_recorder_create(NULL, session->media.video_pt_name, filename);
						} else {
							/* Build a filename */
							g_snprintf(filename, 255, "nosip-%p-%"SCNi64"-peer-video", session, now);
							/* FIXME This only works if offer/answer happened */
							rc = janus_recorder_create(NULL, session->media.video_pt_name, filename);
						}
						/* TODO We should send a FIR/PLI to this peer... */
						if(rc == NULL) {
							/* FIXME We should notify the fact the recorder could not be created */
							JANUS_LOG(LOG_ERR, "Couldn't open an video recording file for this peer!\n");
						} else {
							session->vrc_peer = rc;
						}
					}
				}
				if(record_audio || record_video) {
					/* Start recording the user's audio and/or video */
					JANUS_LOG(LOG_INFO, "Starting recording of user's %s (%p)\n",
						(record_audio && record_video ? "audio and video" : (record_audio ? "audio" : "video")), session);
					if(record_audio) {
						memset(filename, 0, 255);
						if(recording_base) {
							/* Use the filename and path we have been provided */
							g_snprintf(filename, 255, "%s-user-audio", recording_base);
							/* FIXME This only works if offer/answer happened */
							rc = janus_recorder_create(NULL, session->media.audio_pt_name, filename);
						} else {
							/* Build a filename */
							g_snprintf(filename, 255, "nosip-%p-%"SCNi64"-own-audio", session, now);
							/* FIXME This only works if offer/answer happened */
							rc = janus_recorder_create(NULL, session->media.audio_pt_name, filename);
						}
						if(rc == NULL) {
							/* FIXME We should notify the fact the recorder could not be created */
							JANUS_LOG(LOG_ERR, "Couldn't open an audio recording file for this user!\n");
						} else {
							/* If RED is in use, take note of it */
							if(session->media.opusred_pt > 0)
								janus_recorder_opusred(rc, session->media.opusred_pt);
							session->arc = rc;
						}
					}
					if(record_video) {
						memset(filename, 0, 255);
						if(recording_base) {
							/* Use the filename and path we have been provided */
							g_snprintf(filename, 255, "%s-user-video", recording_base);
							/* FIXME This only works if offer/answer happened */
							rc = janus_recorder_create(NULL, session->media.video_pt_name, filename);
						} else {
							/* Build a filename */
							g_snprintf(filename, 255, "nosip-%p-%"SCNi64"-own-video", session, now);
							/* FIXME This only works if offer/answer happened */
							rc = janus_recorder_create(NULL, session->media.video_pt_name, filename);
						}
						if(rc == NULL) {
							/* FIXME We should notify the fact the recorder could not be created */
							JANUS_LOG(LOG_ERR, "Couldn't open a video recording file for this user!\n");
						} else {
							session->vrc = rc;
						}
						/* Send a PLI */
						JANUS_LOG(LOG_VERB, "Recording video, sending a PLI to kickstart it\n");
						gateway->send_pli(session->handle);
					}
				}
			} else {
				/* Stop recording something: notice that this never returns an error, even when we were not recording anything */
				janus_nosip_recorder_close(session, record_audio, record_peer_audio, record_video, record_peer_video);
			}
			janus_mutex_unlock(&session->rec_mutex);
			/* Notify the result */
			result = json_object();
			json_object_set_new(result, "event", json_string("recordingupdated"));
		} else if(!strcasecmp(request_text, "keyframe")) {
			/* Programmatically send a keyframe request via RTCP PLI to
			 * either the WebRTC user, the SIP peer, or both of them */
			JANUS_VALIDATE_JSON_OBJECT(root, keyframe_parameters,
				error_code, error_cause, TRUE,
				JANUS_NOSIP_ERROR_MISSING_ELEMENT, JANUS_NOSIP_ERROR_INVALID_ELEMENT);
			if(error_code != 0)
				goto error;
			gboolean user = json_is_true(json_object_get(root, "user"));
			gboolean peer = json_is_true(json_object_get(root, "peer"));
			if(user) {
				/* Send a PLI to the WebRTC user */
				gateway->send_pli(session->handle);
			}
			if(peer) {
				/* Send a PLI to the SIP peer (but only if they negotiated it) */
				if(session->media.video_pli_supported)
					janus_nosip_rtcp_pli_send(session);
			}
			/* Notify the result */
			result = json_object();
			json_object_set_new(result, "event", json_string("keyframesent"));
		} else {
			JANUS_LOG(LOG_ERR, "Unknown request (%s)\n", request_text);
			error_code = JANUS_NOSIP_ERROR_INVALID_REQUEST;
			g_snprintf(error_cause, 512, "Unknown request (%s)", request_text);
			goto error;
		}

		/* Prepare JSON event */
		json_t *event = json_object();
		json_object_set_new(event, "nosip", json_string("event"));
		if(result != NULL)
			json_object_set_new(event, "result", result);
		int ret = gateway->push_event(msg->handle, &janus_nosip_plugin, msg->transaction, event, localjsep);
		JANUS_LOG(LOG_VERB, "  >> Pushing event: %d (%s)\n", ret, janus_get_api_error(ret));
		json_decref(event);
		if(localjsep)
			json_decref(localjsep);
		janus_nosip_message_free(msg);
		continue;

error:
		{
			/* Prepare JSON error event */
			json_t *event = json_object();
			json_object_set_new(event, "nosip", json_string("event"));
			json_object_set_new(event, "error_code", json_integer(error_code));
			json_object_set_new(event, "error", json_string(error_cause));
			int ret = gateway->push_event(msg->handle, &janus_nosip_plugin, msg->transaction, event, NULL);
			JANUS_LOG(LOG_VERB, "  >> Pushing event: %d (%s)\n", ret, janus_get_api_error(ret));
			json_decref(event);
			janus_nosip_message_free(msg);
		}
	}
	JANUS_LOG(LOG_VERB, "Leaving NoSIP handler thread\n");
	return NULL;
}


void janus_nosip_sdp_process(janus_nosip_session *session, janus_sdp *sdp, gboolean answer, gboolean update, gboolean *changed) {
	if(!session || !sdp)
		return;
	/* c= */
	int opusred_pt = answer ? janus_sdp_get_opusred_pt(sdp, -1) : -1;
	if(sdp->c_addr) {
		if(update) {
			if (changed && (!session->media.remote_audio_ip || strcmp(sdp->c_addr, session->media.remote_audio_ip)))
				/* This is an update and an address changed */
				*changed = TRUE;
			if (changed && (!session->media.remote_video_ip || strcmp(sdp->c_addr, session->media.remote_video_ip)))
				/* This is an update and an address changed */
				*changed = TRUE;
		}
		/* Regardless if we audio and video are being negotiated we set their connection addresses
		 * from session level c= header by default. If media level connection addresses are available
		 * they will be set when processing appropriate media description.*/
		g_free(session->media.remote_audio_ip);
		session->media.remote_audio_ip = g_strdup(sdp->c_addr);
		g_free(session->media.remote_video_ip);
		session->media.remote_video_ip = g_strdup(sdp->c_addr);
	}
	GList *temp = sdp->m_lines;
	while(temp) {
		janus_sdp_mline *m = (janus_sdp_mline *)temp->data;
		session->media.require_srtp = session->media.require_srtp || (m->proto && !strcasecmp(m->proto, "RTP/SAVP"));
		if(m->type == JANUS_SDP_AUDIO) {
			if(m->port) {
				if(m->port != session->media.remote_audio_rtp_port) {
					/* This is an update and an address changed */
					if(changed)
						*changed = TRUE;
				}
				session->media.has_audio = TRUE;
				session->media.remote_audio_rtp_port = m->port;
				session->media.remote_audio_rtcp_port = m->port+1;	/* FIXME We're assuming RTCP is on the next port */
				if(m->direction == JANUS_SDP_SENDONLY || m->direction == JANUS_SDP_INACTIVE)
					session->media.audio_send = FALSE;
				else
					session->media.audio_send = TRUE;
			} else {
				session->media.audio_send = FALSE;
			}
		} else if(m->type == JANUS_SDP_VIDEO) {
			if(m->port) {
				if(m->port != session->media.remote_video_rtp_port) {
					/* This is an update and an address changed */
					if(changed)
						*changed = TRUE;
				}
				session->media.has_video = TRUE;
				session->media.remote_video_rtp_port = m->port;
				session->media.remote_video_rtcp_port = m->port+1;	/* FIXME We're assuming RTCP is on the next port */
				if(m->direction == JANUS_SDP_SENDONLY || m->direction == JANUS_SDP_INACTIVE)
					session->media.video_send = FALSE;
				else
					session->media.video_send = TRUE;
			} else {
				session->media.video_send = FALSE;
			}
		} else {
			JANUS_LOG(LOG_WARN, "Unsupported media line (not audio/video)\n");
			temp = temp->next;
			continue;
		}
		if(m->c_addr && m->type == JANUS_SDP_AUDIO) {
			if(update && (!session->media.remote_audio_ip || strcmp(m->c_addr, session->media.remote_audio_ip))) {
				/* This is an update and an address changed */
				if(changed)
					*changed = TRUE;
			}
			g_free(session->media.remote_audio_ip);
			session->media.remote_audio_ip = g_strdup(m->c_addr);
		}
		else if (m->c_addr && m->type == JANUS_SDP_VIDEO) {
			if(update && (!session->media.remote_video_ip || strcmp(m->c_addr, session->media.remote_video_ip))) {
				/* This is an update and an address changed */
				if(changed)
					*changed = TRUE;
			}
			g_free(session->media.remote_video_ip);
			session->media.remote_video_ip = g_strdup(m->c_addr);
		}
		GList *tempA = m->attributes;
		while(tempA) {
			janus_sdp_attribute *a = (janus_sdp_attribute *)tempA->data;
			if(a->name) {
				if(!strcasecmp(a->name, "crypto")) {
					if(m->type == JANUS_SDP_AUDIO || m->type == JANUS_SDP_VIDEO) {
						if((m->type == JANUS_SDP_AUDIO && session->media.audio_srtp_in != NULL) || (m->type == JANUS_SDP_VIDEO && session->media.video_srtp_in != NULL)) {
							/* Remote SRTP is already set */
							tempA = tempA->next;
							continue;
						}
						gint32 tag = 0;
						char profile[101], crypto[101];
						int res = a->value ? (sscanf(a->value, "%"SCNi32" %100s inline:%100s",
							&tag, profile, crypto)) : 0;
						if(res != 3) {
							JANUS_LOG(LOG_WARN, "Failed to parse crypto line, ignoring... %s\n", a->value);
						} else {
							gboolean video = (m->type == JANUS_SDP_VIDEO);
							if(answer && ((!video && tag != session->media.audio_srtp_tag) || (video && tag != session->media.video_srtp_tag))) {
								/* Not the tag for the crypto line we offered */
								tempA = tempA->next;
								continue;
							}
							if(janus_nosip_srtp_set_remote(session, video, profile, crypto) < 0) {
								/* Unsupported profile? */
								tempA = tempA->next;
								continue;
							}
							if(!video) {
								session->media.audio_srtp_tag = tag;
							} else {
								session->media.video_srtp_tag = tag;
							}
							session->media.has_srtp_remote = TRUE;
						}
					}
				} else if(m->type == JANUS_SDP_VIDEO && !strcasecmp(a->name, "rtcp-fb") && a->value) {
					if(strstr(a->value, " pli"))
						session->media.video_pli_supported = TRUE;
				}
			}
			tempA = tempA->next;
		}
		if(answer && (m->type == JANUS_SDP_AUDIO || m->type == JANUS_SDP_VIDEO)) {
			/* Check which codec was negotiated eventually */
			int pt = -1;
			if(m->ptypes)
				pt = GPOINTER_TO_INT(m->ptypes->data);
			if(pt > -1) {
				if(m->type == JANUS_SDP_AUDIO) {
					if(pt == opusred_pt) {
						session->media.opusred_pt = pt;
						session->media.audio_pt = m->ptypes->next ? GPOINTER_TO_INT(m->ptypes->next->data) : -1;
					} else {
						session->media.audio_pt = pt;
					}
					session->media.audio_pt_name = janus_sdp_get_codec_name(sdp, m->index, session->media.audio_pt);
				} else {
					session->media.video_pt = pt;
					session->media.video_pt_name = janus_sdp_get_codec_name(sdp, m->index, pt);
				}
			}
		}
		temp = temp->next;
	}
	if(update && changed && *changed) {
		/* Something changed: mark this on the session, so that the thread can update the sockets */
		session->media.updated = TRUE;
		if(session->media.pipefd[1] > 0) {
			int code = 1;
			ssize_t res = 0;
			do {
				res = write(session->media.pipefd[1], &code, sizeof(int));
			} while(res == -1 && errno == EINTR);
		}
	}
}

char *janus_nosip_sdp_manipulate(janus_nosip_session *session, janus_sdp *sdp, gboolean answer) {
	if(!session || !sdp)
		return NULL;
	/* Start replacing stuff */
	JANUS_LOG(LOG_VERB, "Setting protocol to %s\n", session->media.require_srtp ? "RTP/SAVP" : "RTP/AVP");
	if(sdp->c_addr) {
		g_free(sdp->c_addr);
		sdp->c_addr = g_strdup(sdp_ip);
	}
	int opusred_pt = answer ? janus_sdp_get_opusred_pt(sdp, -1) : -1;
	GList *temp = sdp->m_lines;
	while(temp) {
		janus_sdp_mline *m = (janus_sdp_mline *)temp->data;
		g_free(m->proto);
		m->proto = g_strdup(session->media.require_srtp ? "RTP/SAVP" : "RTP/AVP");
		if(m->type == JANUS_SDP_AUDIO) {
			m->port = session->media.local_audio_rtp_port;
			if(session->media.has_srtp_local) {
				if(!session->media.audio_srtp_local_profile || !session->media.audio_srtp_local_crypto) {
					janus_nosip_srtp_set_local(session, FALSE, &session->media.audio_srtp_local_profile, &session->media.audio_srtp_local_crypto);
				}
				if(session->media.audio_srtp_tag == 0)
					session->media.audio_srtp_tag = 1;
				janus_sdp_attribute *a = janus_sdp_attribute_create("crypto", "%"SCNi32" %s inline:%s",
					session->media.audio_srtp_tag, session->media.audio_srtp_local_profile, session->media.audio_srtp_local_crypto);
				m->attributes = g_list_append(m->attributes, a);
			}
		} else if(m->type == JANUS_SDP_VIDEO) {
			m->port = session->media.local_video_rtp_port;
			if(session->media.has_srtp_local) {
				if(!session->media.video_srtp_local_profile || !session->media.video_srtp_local_crypto) {
					janus_nosip_srtp_set_local(session, TRUE, &session->media.video_srtp_local_profile, &session->media.video_srtp_local_crypto);
				}
				if(session->media.video_srtp_tag == 0)
					session->media.video_srtp_tag = 1;
				janus_sdp_attribute *a = janus_sdp_attribute_create("crypto", "%"SCNi32" %s inline:%s",
					session->media.video_srtp_tag, session->media.video_srtp_local_profile, session->media.video_srtp_local_crypto);
				m->attributes = g_list_append(m->attributes, a);
			}
		}
		g_free(m->c_addr);
		m->c_addr = g_strdup(sdp_ip ? sdp_ip : local_ip);
		if(answer && (m->type == JANUS_SDP_AUDIO || m->type == JANUS_SDP_VIDEO)) {
			/* Check which codec was negotiated eventually */
			int pt = -1;
			if(m->ptypes)
				pt = GPOINTER_TO_INT(m->ptypes->data);
			if(pt > -1) {
				if(m->type == JANUS_SDP_AUDIO) {
					if(pt == opusred_pt) {
						session->media.opusred_pt = pt;
						session->media.audio_pt = m->ptypes->next ? GPOINTER_TO_INT(m->ptypes->next->data) : -1;
					} else {
						session->media.audio_pt = pt;
					}
					session->media.audio_pt_name = janus_sdp_get_codec_name(sdp, m->index, session->media.audio_pt);
				} else {
					session->media.video_pt = pt;
					session->media.video_pt_name = janus_sdp_get_codec_name(sdp, m->index, pt);
				}
			}
		}
		temp = temp->next;
	}
	/* Generate a SDP string out of our changes */
	return janus_sdp_write(sdp);
}

static int janus_nosip_bind_socket(int fd, int port) {
	gboolean use_ipv6_address_family = !ipv6_disabled &&
		(janus_network_address_is_null(&janus_network_local_ip) || janus_network_local_ip.family == AF_INET6);
	socklen_t addrlen = use_ipv6_address_family? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);
	struct sockaddr_storage rtp_address = { 0 };
	if(use_ipv6_address_family) {
		struct sockaddr_in6 *addr = (struct sockaddr_in6 *)&rtp_address;
		addr->sin6_family = AF_INET6;
		addr->sin6_port = htons(port);
		addr->sin6_addr = janus_network_address_is_null(&janus_network_local_ip) ? in6addr_any : janus_network_local_ip.ipv6;
	} else {
		struct sockaddr_in *addr = (struct sockaddr_in *)&rtp_address;
		addr->sin_family = AF_INET;
		addr->sin_port = htons(port);
		addr->sin_addr.s_addr = janus_network_address_is_null(&janus_network_local_ip) ? INADDR_ANY : janus_network_local_ip.ipv4.s_addr;
	}
	if(bind(fd, (struct sockaddr *)(&rtp_address), addrlen) < 0) {
		JANUS_LOG(LOG_ERR, "Bind failed (port %d), error (%s)\n", port, g_strerror(errno));
		return -1;
	}
	return 0;
}

/* Bind RTP/RTCP port pair */
static int janus_nosip_allocate_port_pair(gboolean video, int fds[2], int ports[2]) {
	uint16_t rtp_port_next = rtp_range_slider; 					/* Read global slider */
	uint16_t rtp_port_start = rtp_port_next;
	gboolean rtp_port_wrap = FALSE;

	gboolean use_ipv6_address_family = !ipv6_disabled &&
		(janus_network_address_is_null(&janus_network_local_ip) || janus_network_local_ip.family == AF_INET6);

	int rtp_fd = -1, rtcp_fd = -1;
	while(1) {
		if(rtp_port_wrap && rtp_port_next >= rtp_port_start) {	/* Full range scanned */
			JANUS_LOG(LOG_ERR, "No ports available for %s channel in range: %u -- %u\n",
				  video ? "video" : "audio", rtp_range_min, rtp_range_max);
			break;
		}
		if(rtp_fd == -1) {
			rtp_fd = socket(use_ipv6_address_family ? AF_INET6 : AF_INET, SOCK_DGRAM, 0);
			int v6only = 0;
			if(use_ipv6_address_family && rtp_fd != -1 && setsockopt(rtp_fd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only)) != 0) {
				JANUS_LOG(LOG_WARN, "Error setting v6only to false on RTP socket (error=%s)\n",
					g_strerror(errno));
			}
			/* Set the DSCP value if set in the config file */
			if(rtp_fd != -1 && !video && dscp_audio_rtp > 0) {
				int optval = dscp_audio_rtp << 2;
				int ret = setsockopt(rtp_fd, IPPROTO_IP, IP_TOS, &optval, sizeof(optval));
				if(ret < 0) {
					JANUS_LOG(LOG_WARN, "Error setting IP_TOS %d on audio RTP socket (error=%s)\n",
						optval, g_strerror(errno));
				}
			} else if(rtp_fd != -1 && video && dscp_video_rtp > 0) {
				int optval = dscp_video_rtp << 2;
				int ret = setsockopt(rtp_fd, IPPROTO_IP, IP_TOS, &optval, sizeof(optval));
				if(ret < 0) {
					JANUS_LOG(LOG_WARN, "Error setting IP_TOS %d on video RTP socket (error=%s)\n",
						optval, g_strerror(errno));
				}
			}
		}
		if(rtcp_fd == -1) {
			int v6only = 0;
			rtcp_fd = socket(use_ipv6_address_family ? AF_INET6 : AF_INET, SOCK_DGRAM, 0);
			if(use_ipv6_address_family && rtcp_fd != -1 && setsockopt(rtcp_fd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only)) != 0) {
				JANUS_LOG(LOG_WARN, "Error setting v6only to false on RTP socket (error=%s)\n",
					g_strerror(errno));
			}
		}
		if(rtp_fd == -1 || rtcp_fd == -1) {
			JANUS_LOG(LOG_ERR, "Error creating %s sockets...\n", video ? "video" : "audio");
			break;
		}
	 	int rtp_port = rtp_port_next;
		int rtcp_port = rtp_port+1;
		if((uint32_t)(rtp_port_next + 2UL) < rtp_range_max) {
			/* Advance to next pair */
			rtp_port_next += 2;
		} else {
			rtp_port_next = rtp_range_min;
			rtp_port_wrap = TRUE;
		}
		if(janus_nosip_bind_socket(rtp_fd, rtp_port)) {
			/* rtp_fd still unbound, reuse it */
		} else if(janus_nosip_bind_socket(rtcp_fd, rtcp_port)) {
			close(rtp_fd);
			rtp_fd = -1;
			/* rtcp_fd still unbound, reuse it */
		} else {
			fds[0] = rtp_fd;
			fds[1] = rtcp_fd;
			ports[0] = rtp_port;
			ports[1] = rtcp_port;
			rtp_range_slider = rtp_port_next;		/* Write global slider */
			return 0;
		}
	}
	if(rtp_fd != -1) {
		close(rtp_fd);
	}
	if(rtcp_fd != -1) {
		close(rtcp_fd);
	}
	return -1;
}
/* Bind local RTP/RTCP sockets */
static int janus_nosip_allocate_local_ports(janus_nosip_session *session, gboolean update) {
	if(session == NULL) {
		JANUS_LOG(LOG_ERR, "Invalid session\n");
		return -1;
	}
	/* Reset status */
	if(!update) {
		if(session->media.audio_rtp_fd != -1) {
			close(session->media.audio_rtp_fd);
			session->media.audio_rtp_fd = -1;
		}
		if(session->media.audio_rtcp_fd != -1) {
			close(session->media.audio_rtcp_fd);
			session->media.audio_rtcp_fd = -1;
		}
		session->media.local_audio_rtp_port = 0;
		session->media.local_audio_rtcp_port = 0;
		session->media.audio_ssrc = 0;
		if(session->media.video_rtp_fd != -1) {
			close(session->media.video_rtp_fd);
			session->media.video_rtp_fd = -1;
		}
		if(session->media.video_rtcp_fd != -1) {
			close(session->media.video_rtcp_fd);
			session->media.video_rtcp_fd = -1;
		}
		session->media.local_video_rtp_port = 0;
		session->media.local_video_rtcp_port = 0;
		session->media.video_ssrc = 0;
		if(session->media.pipefd[0] > 0) {
			close(session->media.pipefd[0]);
			session->media.pipefd[0] = -1;
		}
		if(session->media.pipefd[1] > 0) {
			close(session->media.pipefd[1]);
			session->media.pipefd[1] = -1;
		}
	}
	/* Start */
	if(session->media.has_audio &&
			(session->media.local_audio_rtp_port == 0 || session->media.local_audio_rtcp_port == 0)) {
		if(session->media.audio_rtp_fd != -1) {
			JANUS_LOG(LOG_WARN, "Audio RTP unbound socket detected, closing ...\n");
			close(session->media.audio_rtp_fd);
			session->media.audio_rtp_fd = -1;
		}
		if(session->media.audio_rtcp_fd != -1) {
			JANUS_LOG(LOG_WARN, "Audio RTCP unbound socket detected, closing ...\n");
			close(session->media.audio_rtcp_fd);
			session->media.audio_rtcp_fd = -1;
		}
		JANUS_LOG(LOG_VERB, "Allocating audio ports:\n");
		int fds[2], ports[2];
		if(janus_nosip_allocate_port_pair(FALSE, fds, ports)) {
			return -1;
		}
		JANUS_LOG(LOG_VERB, "Audio RTP listener bound to port %d\n", ports[0]);
		JANUS_LOG(LOG_VERB, "Audio RTCP listener bound to port %d\n", ports[1]);
		session->media.audio_rtp_fd = fds[0];
		session->media.audio_rtcp_fd = fds[1];
		session->media.local_audio_rtp_port = ports[0];
		session->media.local_audio_rtcp_port = ports[1];
	}
	if(session->media.has_video &&
			(session->media.local_video_rtp_port == 0 || session->media.local_video_rtcp_port == 0)) {
		if(session->media.video_rtp_fd != -1) {
			JANUS_LOG(LOG_WARN, "Video RTP unbound socket detected, closing ...\n");
			close(session->media.video_rtp_fd);
			session->media.video_rtp_fd = -1;
		}
		if(session->media.video_rtcp_fd != -1) {
			JANUS_LOG(LOG_WARN, "Video RTCP unbound socket detected, closing ...\n");
			close(session->media.video_rtcp_fd);
			session->media.video_rtcp_fd = -1;
		}
		JANUS_LOG(LOG_VERB, "Allocating video ports:\n");
		int fds[2], ports[2];
		if(janus_nosip_allocate_port_pair(TRUE, fds, ports)) {
			return -1;
		}
		JANUS_LOG(LOG_VERB, "Video RTP listener bound to port %d\n", ports[0]);
		JANUS_LOG(LOG_VERB, "Video RTCP listener bound to port %d\n", ports[1]);
		session->media.video_rtp_fd = fds[0];
		session->media.video_rtcp_fd = fds[1];
		session->media.local_video_rtp_port = ports[0];
		session->media.local_video_rtcp_port = ports[1];
	}
	/* We need this to quickly interrupt the poll when it's time to update a session or wrap up */
	if(!update) {
		pipe(session->media.pipefd);
	} else {
		/* Something changed: mark this on the session, so that the thread can update the sockets */
		session->media.updated = TRUE;
		if(session->media.pipefd[1] > 0) {
			int code = 1;
			ssize_t res = 0;
			do {
				res = write(session->media.pipefd[1], &code, sizeof(int));
			} while(res == -1 && errno == EINTR);
		}
	}
	return 0;
}

/* Helper method to (re)connect RTP/RTCP sockets */
static void janus_nosip_connect_sockets(janus_nosip_session *session, struct sockaddr_storage *audio_server_addr, struct sockaddr_storage *video_server_addr) {
	if(!session || (!audio_server_addr && !video_server_addr))
		return;

	if(session->media.updated) {
		JANUS_LOG(LOG_VERB, "Updating session sockets\n");
	}

	/* Connect peers (FIXME This pretty much sucks right now) */
	if(session->media.remote_audio_rtp_port && audio_server_addr && session->media.audio_rtp_fd != -1) {
		if(audio_server_addr->ss_family == AF_INET6) {
			struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)audio_server_addr;
			addr6->sin6_port = htons(session->media.remote_audio_rtp_port);
		} else if(audio_server_addr->ss_family == AF_INET) {
			struct sockaddr_in *addr = (struct sockaddr_in *)audio_server_addr;
			addr->sin_port = htons(session->media.remote_audio_rtp_port);
		}
		if(connect(session->media.audio_rtp_fd, (struct sockaddr *)audio_server_addr, sizeof(struct sockaddr_storage)) == -1) {
			JANUS_LOG(LOG_ERR, "[NoSIP-%p] Couldn't connect audio RTP? (%s:%d)\n", session,
				session->media.remote_audio_ip, session->media.remote_audio_rtp_port);
			JANUS_LOG(LOG_ERR, "[NoSIP-%p]   -- %d (%s)\n", session, errno, g_strerror(errno));
		}
	}
	if(session->media.remote_audio_rtcp_port && audio_server_addr && session->media.audio_rtcp_fd != -1) {
		if(audio_server_addr->ss_family == AF_INET6) {
			struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)audio_server_addr;
			addr6->sin6_port = htons(session->media.remote_audio_rtcp_port);
		} else if(audio_server_addr->ss_family == AF_INET) {
			struct sockaddr_in *addr = (struct sockaddr_in *)audio_server_addr;
			addr->sin_port = htons(session->media.remote_audio_rtcp_port);
		}
		if(connect(session->media.audio_rtcp_fd, (struct sockaddr *)audio_server_addr, sizeof(struct sockaddr_storage)) == -1) {
			JANUS_LOG(LOG_ERR, "[NoSIP-%p] Couldn't connect audio RTCP? (%s:%d)\n", session,
				session->media.remote_audio_ip, session->media.remote_audio_rtcp_port);
			JANUS_LOG(LOG_ERR, "[NoSIP-%p]   -- %d (%s)\n", session, errno, g_strerror(errno));
		}
	}
	if(session->media.remote_video_rtp_port && video_server_addr && session->media.video_rtp_fd != -1) {
		if(video_server_addr->ss_family == AF_INET6) {
			struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)video_server_addr;
			addr6->sin6_port = htons(session->media.remote_video_rtp_port);
		} else if(video_server_addr->ss_family == AF_INET) {
			struct sockaddr_in *addr = (struct sockaddr_in *)video_server_addr;
			addr->sin_port = htons(session->media.remote_video_rtp_port);
		}
		if(connect(session->media.video_rtp_fd, (struct sockaddr *)video_server_addr, sizeof(struct sockaddr_storage)) == -1) {
			JANUS_LOG(LOG_ERR, "[NoSIP-%p] Couldn't connect video RTP? (%s:%d)\n", session,
				session->media.remote_video_ip, session->media.remote_video_rtp_port);
			JANUS_LOG(LOG_ERR, "[NoSIP-%p]   -- %d (%s)\n", session, errno, g_strerror(errno));
		}
	}
	if(session->media.remote_video_rtcp_port && video_server_addr && session->media.video_rtcp_fd != -1) {
		if(video_server_addr->ss_family == AF_INET6) {
			struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)video_server_addr;
			addr6->sin6_port = htons(session->media.remote_video_rtcp_port);
		} else if(video_server_addr->ss_family == AF_INET) {
			struct sockaddr_in *addr = (struct sockaddr_in *)video_server_addr;
			addr->sin_port = htons(session->media.remote_video_rtcp_port);
		}
		if(connect(session->media.video_rtcp_fd, (struct sockaddr *)video_server_addr, sizeof(struct sockaddr_storage)) == -1) {
			JANUS_LOG(LOG_ERR, "[NoSIP-%p] Couldn't connect video RTCP? (%s:%d)\n", session,
				session->media.remote_video_ip, session->media.remote_video_rtcp_port);
			JANUS_LOG(LOG_ERR, "[NoSIP-%p]   -- %d (%s)\n", session, errno, g_strerror(errno));
		}
	}

}

static void janus_nosip_media_cleanup(janus_nosip_session *session) {
	if(session->media.audio_rtp_fd != -1) {
		close(session->media.audio_rtp_fd);
		session->media.audio_rtp_fd = -1;
	}
	if(session->media.audio_rtcp_fd != -1) {
		close(session->media.audio_rtcp_fd);
		session->media.audio_rtcp_fd = -1;
	}
	session->media.local_audio_rtp_port = 0;
	session->media.local_audio_rtcp_port = 0;
	session->media.remote_audio_rtp_port = 0;
	session->media.remote_audio_rtcp_port = 0;
	session->media.audio_ssrc = 0;
	session->media.audio_ssrc_peer = 0;
	if(session->media.video_rtp_fd != -1) {
		close(session->media.video_rtp_fd);
		session->media.video_rtp_fd = -1;
	}
	if(session->media.video_rtcp_fd != -1) {
		close(session->media.video_rtcp_fd);
		session->media.video_rtcp_fd = -1;
	}
	session->media.local_video_rtp_port = 0;
	session->media.local_video_rtcp_port = 0;
	session->media.remote_video_rtp_port = 0;
	session->media.remote_video_rtcp_port = 0;
	session->media.video_ssrc = 0;
	session->media.video_ssrc_peer = 0;
	session->media.simulcast_ssrc = 0;
	if(session->media.pipefd[0] > 0) {
		close(session->media.pipefd[0]);
		session->media.pipefd[0] = -1;
	}
	if(session->media.pipefd[1] > 0) {
		close(session->media.pipefd[1]);
		session->media.pipefd[1] = -1;
	}
	/* Clean up SRTP stuff, if needed */
	janus_nosip_srtp_cleanup(session);

	/* Media fields not cleaned up elsewhere */
	janus_nosip_media_reset(session);
}

/* Thread to relay RTP/RTCP frames coming from the peer */
static void *janus_nosip_relay_thread(void *data) {
	janus_nosip_session *session = (janus_nosip_session *)data;
	if(!session) {
		g_thread_unref(g_thread_self());
		return NULL;
	}
	JANUS_LOG(LOG_INFO, "[NoSIP-%p] Starting relay thread\n", session);

	/* File descriptors */
	socklen_t addrlen;
	struct sockaddr_in remote = { 0 };
	int resfd = 0, bytes = 0, pollerrs = 0;
	struct pollfd fds[5];
	int pipe_fd = session->media.pipefd[0];
	char buffer[1500];
	memset(buffer, 0, 1500);
	if(pipe_fd == -1) {
		/* If the pipe file descriptor doesn't exist, it means we're done already,
		 * and/or we may never be notified about sessions being closed, so give up */
		JANUS_LOG(LOG_WARN, "[NoSIP-%p] Leaving thread, no pipe file descriptor...\n", session);
		janus_refcount_decrease(&session->ref);
		g_thread_unref(g_thread_self());
		return NULL;
	}
	/* Loop */
	int num = 0;
	gboolean goon = TRUE;

	session->media.updated = TRUE; /* Connect UDP sockets upon loop entry */
	gboolean have_audio_server_ip = TRUE;
	gboolean have_video_server_ip = TRUE;

	while(goon && session != NULL &&
			!g_atomic_int_get(&session->destroyed) && !g_atomic_int_get(&session->hangingup)) {

		if(session->media.updated) {
			/* Apparently there was a session update, or the loop has just been entered */
			session->media.updated = FALSE;

			/* Resolve the addresses, if needed */
			have_audio_server_ip = FALSE;
			have_video_server_ip = FALSE;
			struct sockaddr_storage audio_server_addr = { 0 }, video_server_addr = { 0 };
			if(session->media.remote_audio_ip && strcmp(session->media.remote_audio_ip, "0.0.0.0")) {
				if(janus_network_resolve_address(session->media.remote_audio_ip, &audio_server_addr) < 0) {
					JANUS_LOG(LOG_ERR, "[NoSIP-%p] Couldn't resolve audio address '%s'\n",
						session, session->media.remote_audio_ip);
				} else {
					/* Address resolved */
					have_audio_server_ip = TRUE;
				}
			}
			if(session->media.remote_video_ip && strcmp(session->media.remote_video_ip, "0.0.0.0")) {
				if(janus_network_resolve_address(session->media.remote_video_ip, &video_server_addr) < 0) {
					JANUS_LOG(LOG_ERR, "[NoSIP-%p] Couldn't resolve video address '%s'\n",
						session, session->media.remote_video_ip);
				} else {
					/* Address resolved */
					have_video_server_ip = TRUE;
				}
			}

			if(have_audio_server_ip || have_video_server_ip) {
				janus_nosip_connect_sockets(session, have_audio_server_ip ? &audio_server_addr : NULL,
					have_video_server_ip ? &video_server_addr : NULL);
			} else if (session->media.remote_audio_ip == NULL && session->media.remote_video_ip == NULL) {
				JANUS_LOG(LOG_ERR, "[NoSIP-%p] Couldn't update session details: both audio and video remote IP addresses are NULL\n", session);
			} else {
				if(session->media.remote_audio_ip)
					JANUS_LOG(LOG_ERR, "[NoSIP-%p] Couldn't update session details: audio remote IP address (%s) is invalid\n",
						session, session->media.remote_audio_ip);
				if(session->media.remote_video_ip)
					JANUS_LOG(LOG_ERR, "[NoSIP-%p] Couldn't update session details: video remote IP address (%s) is invalid\n",
						session, session->media.remote_video_ip);
			}
		}

		/* Prepare poll */
		num = 0;
		if(session->media.audio_rtp_fd != -1) {
			fds[num].fd = session->media.audio_rtp_fd;
			fds[num].events = POLLIN;
			fds[num].revents = 0;
			num++;
		}
		if(session->media.audio_rtcp_fd != -1) {
			fds[num].fd = session->media.audio_rtcp_fd;
			fds[num].events = POLLIN;
			fds[num].revents = 0;
			num++;
		}
		if(session->media.video_rtp_fd != -1) {
			fds[num].fd = session->media.video_rtp_fd;
			fds[num].events = POLLIN;
			fds[num].revents = 0;
			num++;
		}
		if(session->media.video_rtcp_fd != -1) {
			fds[num].fd = session->media.video_rtcp_fd;
			fds[num].events = POLLIN;
			fds[num].revents = 0;
			num++;
		}
		/* Finally, let's add the pipe */
		pipe_fd = session->media.pipefd[0];
		if(pipe_fd == -1) {
			/* Pipe was closed? Means the call is over */
			break;
		}
		fds[num].fd = pipe_fd;
		fds[num].events = POLLIN;
		fds[num].revents = 0;
		num++;
		/* Wait for some data */
		resfd = poll(fds, num, 1000);
		if(resfd < 0) {
			if(errno == EINTR) {
				JANUS_LOG(LOG_HUGE, "[NoSIP-%p] Got an EINTR (%s), ignoring...\n", session, g_strerror(errno));
				continue;
			}
			JANUS_LOG(LOG_ERR, "[NoSIP-%p] Error polling...\n", session);
			JANUS_LOG(LOG_ERR, "[NoSIP-%p]   -- %d (%s)\n", session, errno, g_strerror(errno));
			break;
		} else if(resfd == 0) {
			/* No data, keep going */
			continue;
		}
		if(session == NULL || g_atomic_int_get(&session->destroyed))
			break;
		int i = 0;
		for(i=0; i<num; i++) {
			if(fds[i].revents & (POLLERR | POLLHUP)) {
				/* If we just updated the session, let's wait until things have calmed down */
				if(session->media.updated)
					break;
				/* Check the socket error */
				int error = 0;
				socklen_t errlen = sizeof(error);
				getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR, (void *)&error, &errlen);
				if(error == 0) {
					/* Maybe not a breaking error after all? */
					continue;
				} else if(error == 111) {
					/* ICMP error? If it's related to RTCP, let's just close the RTCP socket and move on */
					if(fds[i].fd == session->media.audio_rtcp_fd) {
						JANUS_LOG(LOG_WARN, "[NoSIP-%p] Got a '%s' on the audio RTCP socket, closing it\n",
							session, g_strerror(error));
						janus_mutex_lock(&session->mutex);
						close(session->media.audio_rtcp_fd);
						session->media.audio_rtcp_fd = -1;
						janus_mutex_unlock(&session->mutex);
					} else if(fds[i].fd == session->media.video_rtcp_fd) {
						JANUS_LOG(LOG_WARN, "[NoSIP-%p] Got a '%s' on the video RTCP socket, closing it\n",
							session, g_strerror(error));
						janus_mutex_lock(&session->mutex);
						close(session->media.video_rtcp_fd);
						session->media.video_rtcp_fd = -1;
						janus_mutex_unlock(&session->mutex);
					}
				}
				/* FIXME Should we be more tolerant of ICMP errors on RTP sockets as well? */
				pollerrs++;
				if(pollerrs < 100)
					continue;
				JANUS_LOG(LOG_ERR, "[NoSIP-%p] Too many errors polling %d (socket #%d): %s...\n", session,
					fds[i].fd, i, fds[i].revents & POLLERR ? "POLLERR" : "POLLHUP");
				JANUS_LOG(LOG_ERR, "[NoSIP-%p]   -- %d (%s)\n", session, error, g_strerror(error));
				/* Can we assume it's pretty much over, after a POLLERR? */
				goon = FALSE;
				/* FIXME Close the PeerConnection */
				gateway->close_pc(session->handle);
				break;
			} else if(fds[i].revents & POLLIN) {
				if(pipe_fd != -1 && fds[i].fd == pipe_fd) {
					/* Poll interrupted for a reason, go on */
					int code = 0;
					(void)read(pipe_fd, &code, sizeof(int));
					break;
				}
				/* Got an RTP/RTCP packet */
				addrlen = sizeof(remote);
				bytes = recvfrom(fds[i].fd, buffer, 1500, 0, (struct sockaddr*)&remote, &addrlen);
				if(bytes < 0) {
					/* Failed to read? */
					continue;
				}
				/* Let's check what this is */
				gboolean video = fds[i].fd == session->media.video_rtp_fd || fds[i].fd == session->media.video_rtcp_fd;
				gboolean rtcp = fds[i].fd == session->media.audio_rtcp_fd || fds[i].fd == session->media.video_rtcp_fd;
				if(!rtcp) {
					/* Audio or Video RTP */
					if(!janus_is_rtp(buffer, bytes)) {
						/* Not an RTP packet? */
						continue;
					}
					pollerrs = 0;
					rtp_header *header = (rtp_header *)buffer;
					if((video && session->media.video_ssrc_peer != ntohl(header->ssrc)) ||
							(!video && session->media.audio_ssrc_peer != ntohl(header->ssrc))) {
						if(video && session->media.video_ssrc_peer == 0) {
							session->media.video_ssrc_peer = ntohl(header->ssrc);
						} else if(!video && session->media.audio_ssrc_peer == 0) {
							session->media.audio_ssrc_peer = ntohl(header->ssrc);
						}
						JANUS_LOG(LOG_VERB, "[NoSIP-%p] Got SIP peer %s SSRC: %"SCNu32"\n",
							session, video ? "video" : "audio",
							video ? session->media.video_ssrc_peer : session->media.audio_ssrc_peer);
					}
					/* Is this SRTP? */
					if(session->media.has_srtp_remote) {
						int buflen = bytes;
						srtp_err_status_t res = srtp_unprotect(
							(video ? session->media.video_srtp_in : session->media.audio_srtp_in),
							buffer, &buflen);
						if(res != srtp_err_status_ok && res != srtp_err_status_replay_fail && res != srtp_err_status_replay_old) {
							guint32 timestamp = ntohl(header->timestamp);
							guint16 seq = ntohs(header->seq_number);
							JANUS_LOG(LOG_ERR, "[NoSIP-%p] %s SRTP unprotect error: %s (len=%d-->%d, ts=%"SCNu32", seq=%"SCNu16")\n",
								session, video ? "Video" : "Audio", janus_srtp_error_str(res), bytes, buflen, timestamp, seq);
							continue;
						}
						bytes = buflen;
					}
					/* Check if the SSRC changed (e.g., after a re-INVITE or UPDATE) */
					janus_rtp_header_update(header, video ? &session->media.vcontext : &session->media.acontext, video, 0);
					/* Save the frame if we're recording */
					header->ssrc = htonl(video ? session->media.video_ssrc_peer : session->media.audio_ssrc_peer);
					janus_recorder_save_frame(video ? session->vrc_peer : session->arc_peer, buffer, bytes);
					/* Relay to browser */
					janus_plugin_rtp rtp = { .mindex = -1, .video = video, .buffer = buffer, .length = bytes };
					/* Add audio-level extension, if present */
					janus_plugin_rtp_extensions_reset(&rtp.extensions);
					if(!video && session->media.audio_level_extension_id != -1) {
						gboolean vad = FALSE;
						int level = -1;
						if(janus_rtp_header_extension_parse_audio_level(buffer, bytes,
								session->media.audio_level_extension_id, &vad, &level) == 0) {
							rtp.extensions.audio_level = level;
							rtp.extensions.audio_level_vad = vad;
						}
					} else if(video && session->media.video_orientation_extension_id > 0) {
						gboolean c = FALSE, f = FALSE, r1 = FALSE, r0 = FALSE;
						if(janus_rtp_header_extension_parse_video_orientation(buffer, bytes,
								session->media.video_orientation_extension_id, &c, &f, &r1, &r0) == 0) {
							rtp.extensions.video_rotation = 0;
							if(r1 && r0)
								rtp.extensions.video_rotation = 270;
							else if(r1)
								rtp.extensions.video_rotation = 180;
							else if(r0)
								rtp.extensions.video_rotation = 90;
							rtp.extensions.video_back_camera = c;
							rtp.extensions.video_flipped = f;
						}
					}
					gateway->relay_rtp(session->handle, &rtp);
					continue;
				} else {
					/* Audio or Video RTCP */
					if(!janus_is_rtcp(buffer, bytes)) {
						/* Not an RTCP packet? */
						continue;
					}
					if(session->media.has_srtp_remote) {
						int buflen = bytes;
						srtp_err_status_t res = srtp_unprotect_rtcp(
							(video ? session->media.video_srtp_in : session->media.audio_srtp_in),
							buffer, &buflen);
						if(res != srtp_err_status_ok && res != srtp_err_status_replay_fail && res != srtp_err_status_replay_old) {
							JANUS_LOG(LOG_ERR, "[NoSIP-%p] %s SRTCP unprotect error: %s (len=%d-->%d)\n",
								session, video ? "Video" : "Audio", janus_srtp_error_str(res), bytes, buflen);
							continue;
						}
						bytes = buflen;
					}
					/* Relay to browser */
					janus_plugin_rtcp rtcp = { .mindex = -1, .video = video, .buffer = buffer, bytes };
					gateway->relay_rtcp(session->handle, &rtcp);
					continue;
				}
			}
		}
	}
	/* Cleanup the media session */
	janus_mutex_lock(&session->mutex);
	janus_nosip_media_cleanup(session);
	janus_mutex_unlock(&session->mutex);
	/* Done */
	JANUS_LOG(LOG_INFO, "Leaving NoSIP relay thread\n");
	session->relayer_thread = NULL;
	janus_refcount_decrease(&session->ref);
	g_thread_unref(g_thread_self());
	return NULL;
}

/* Helper method to send an RTCP PLI to the peer */
static void janus_nosip_rtcp_pli_send(janus_nosip_session *session) {
	if(!session || g_atomic_int_get(&session->destroyed)) {
		JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
		return;
	}
	if(!session->media.has_video || session->media.video_rtcp_fd == -1)
		return;
	/* Generate a PLI */
	char rtcp_buf[12];
	int rtcp_len = 12;
	janus_rtcp_pli((char *)&rtcp_buf, rtcp_len);
	/* Fix SSRCs as the Janus core does */
	JANUS_LOG(LOG_HUGE, "[NoSIP-%p] Fixing SSRCs (local %u, peer %u)\n",
		session, session->media.video_ssrc, session->media.video_ssrc_peer);
	janus_rtcp_fix_ssrc(NULL, (char *)rtcp_buf, rtcp_len, 1, session->media.video_ssrc, session->media.video_ssrc_peer);
	/* Is SRTP involved? */
	if(session->media.has_srtp_local) {
		char sbuf[50];
		memcpy(&sbuf, rtcp_buf, rtcp_len);
		int protected = rtcp_len;
		int res = srtp_protect_rtcp(session->media.video_srtp_out, &sbuf, &protected);
		if(res != srtp_err_status_ok) {
			JANUS_LOG(LOG_ERR, "[NoSIP-%p] Video SRTCP protect error... %s (len=%d-->%d)...\n",
				session, janus_srtp_error_str(res), rtcp_len, protected);
		} else {
			/* Forward the message to the peer */
			if(send(session->media.video_rtcp_fd, sbuf, protected, 0) < 0) {
				JANUS_LOG(LOG_HUGE, "[NoSIP-%p] Error sending SRTCP video packet... %s (len=%d)...\n",
					session, g_strerror(errno), protected);
			}
		}
	} else {
		/* Forward the message to the peer */
		if(send(session->media.video_rtcp_fd, rtcp_buf, rtcp_len, 0) < 0) {
			JANUS_LOG(LOG_HUGE, "[NoSIP-%p] Error sending RTCP video packet... %s (len=%d)...\n",
				session, g_strerror(errno), rtcp_len);
		}
	}
}
