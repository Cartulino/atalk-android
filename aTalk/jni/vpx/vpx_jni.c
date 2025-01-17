/*
 * Copyright @ 2015 Atlassian Pty Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <jni.h>

#include <stdlib.h>
#include <string.h>

#include <pthread.h>
#include <unistd.h>

#include <vpx/vpx_decoder.h>
#include <vpx/vpx_encoder.h>
#include <vpx/vpx_image.h>
#include <vpx/vp8dx.h>
#include <vpx/vp8cx.h>

#include <android/log.h>
#define TAG "vpx_jni" // This is the logo of a custom LOG
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,TAG ,__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,TAG ,__VA_ARGS__)

/*
 * Both openjdk-1.7's jni_md.h and vpx/vpx_codec.h define the 'UNUSED' macro.
 * Include this here, after the vpx includes, because it brings in jni_md.h, and
 * using this order at least allows for successful compilation.
 */

/* unused defines
#undef org_atalk_impl_neomedia_codec_video_VPX_CODEC_OK
#define org_atalk_impl_neomedia_codec_video_VPX_CODEC_OK 0L
#undef org_atalk_impl_neomedia_codec_video_VPX_CODEC_LIST_END
#define org_atalk_impl_neomedia_codec_video_VPX_CODEC_LIST_END 9L
#undef org_atalk_impl_neomedia_codec_video_VPX_CODEC_USE_XMA
#define org_atalk_impl_neomedia_codec_video_VPX_CODEC_USE_XMA 1L
#undef org_atalk_impl_neomedia_codec_video_VPX_CODEC_USE_OUTPUT_PARTITION
#define org_atalk_impl_neomedia_codec_video_VPX_CODEC_USE_OUTPUT_PARTITION 131072L
#undef org_atalk_impl_neomedia_codec_video_VPX_IMG_FMT_I420
#define org_atalk_impl_neomedia_codec_video_VPX_IMG_FMT_I420 258L
#undef org_atalk_impl_neomedia_codec_video_VPX_RC_MODE_VBR
#define org_atalk_impl_neomedia_codec_video_VPX_RC_MODE_VBR 0L
#undef org_atalk_impl_neomedia_codec_video_VPX_RC_MODE_CBR
#define org_atalk_impl_neomedia_codec_video_VPX_RC_MODE_CBR 1L
#undef org_atalk_impl_neomedia_codec_video_VPX_RC_MODE_CQ
#define org_atalk_impl_neomedia_codec_video_VPX_RC_MODE_CQ 2L
#undef org_atalk_impl_neomedia_codec_video_VPX_KF_MODE_AUTO
#define org_atalk_impl_neomedia_codec_video_VPX_KF_MODE_AUTO 1Lfv
#undef org_atalk_impl_neomedia_codec_video_VPX_KF_MODE_DISABLED
#define org_atalk_impl_neomedia_codec_video_VPX_KF_MODE_DISABLED 1L
#undef org_atalk_impl_neomedia_codec_video_VPX_DL_REALTIME
#define org_atalk_impl_neomedia_codec_video_VPX_DL_REALTIME 1L
#undef org_atalk_impl_neomedia_codec_video_VPX_CODEC_CX_FRAME_PKT
#define org_atalk_impl_neomedia_codec_video_VPX_CODEC_CX_FRAME_PKT 0L
#define VPX_CODEC_DISABLE_COMPAT 1
*/

#undef org_atalk_impl_neomedia_codec_video_VPX_INTERFACE_VP8_DEC
#define org_atalk_impl_neomedia_codec_video_VPX_INTERFACE_VP8_DEC 0L
#undef org_atalk_impl_neomedia_codec_video_VPX_INTERFACE_VP8_ENC
#define org_atalk_impl_neomedia_codec_video_VPX_INTERFACE_VP8_ENC 1L
#undef org_atalk_impl_neomedia_codec_video_VPX_INTERFACE_VP9_DEC
#define org_atalk_impl_neomedia_codec_video_VPX_INTERFACE_VP9_DEC 2L
#undef org_atalk_impl_neomedia_codec_video_VPX_INTERFACE_VP9_ENC
#define org_atalk_impl_neomedia_codec_video_VPX_INTERFACE_VP9_ENC 3L

/* Convert the INTERFACE_* constants defined in java to the (vpx_codec_iface_t *)'s used in libvpx */
#define GET_INTERFACE(x) \
     (((x) == org_atalk_impl_neomedia_codec_video_VPX_INTERFACE_VP8_DEC) \
     ? vpx_codec_vp8_dx() \
     : (((x) == org_atalk_impl_neomedia_codec_video_VPX_INTERFACE_VP8_ENC)) \
         ? vpx_codec_vp8_cx() \
         : (((x) == org_atalk_impl_neomedia_codec_video_VPX_INTERFACE_VP9_DEC)) \
             ? vpx_codec_vp9_dx() \
             : (((x) == org_atalk_impl_neomedia_codec_video_VPX_INTERFACE_VP9_ENC)) \
                 ? vpx_codec_vp9_cx() \
                 : NULL)

#define DEFINE_ENC_CFG_INT_PROPERTY_SETTER(name, property) \
    JNIEXPORT void JNICALL Java_org_atalk_impl_neomedia_codec_video_VPX_codec_1enc_1cfg_1set_1##name \
            (JNIEnv *env, jclass clazz, jlong cfg, jint value);     \
    JNIEXPORT void JNICALL Java_org_atalk_impl_neomedia_codec_video_VPX_codec_1enc_1cfg_1set_1##name \
            (JNIEnv *env, jclass clazz, jlong cfg, jint value) \
        { \
            ((vpx_codec_enc_cfg_t *) (intptr_t) cfg)->property = (int) value; \
        }

#define DEFINE_IMG_INT_PROPERTY_SETTER(name, property) \
    JNIEXPORT void JNICALL Java_org_atalk_impl_neomedia_codec_video_VPX_img_1set_1##name \
            (JNIEnv *env, jclass clazz, jlong img, jint value);       \
    JNIEXPORT void JNICALL Java_org_atalk_impl_neomedia_codec_video_VPX_img_1set_1##name \
            (JNIEnv *env, jclass clazz, jlong img, jint value) \
        { \
            ((vpx_image_t *) (intptr_t) img)->property = (int) value; \
        }

/*
 * Class:     org_atalk_impl_neomedia_codec_video_VPX
 * Method:    NAME
 */
#define VPX_FUNC(RETURN_TYPE, NAME, ...) \
  JNIEXPORT RETURN_TYPE JNICALL Java_org_atalk_impl_neomedia_codec_video_VPX_ ## NAME \
                      (JNIEnv * env, jobject thiz, ##__VA_ARGS__);                    \
  JNIEXPORT RETURN_TYPE JNICALL Java_org_atalk_impl_neomedia_codec_video_VPX_ ## NAME \
                      (JNIEnv * env, jobject thiz, ##__VA_ARGS__)

/* ===========================================
 * https://codelab.wordpress.com/2014/11/03/how-to-use-standard-output-streams-for-logging-in-android-apps/
 * Allow display of the fprintf(stderr, ...) in jniLib ins logcat
 *
 */
static int pfd[2];
static pthread_t thr;
static const char *tag = "myapp";

static void *thread_func(void* null)
{
    ssize_t rdsz;
    char buf[128];
    while((rdsz = read(pfd[0], buf, sizeof buf - 1)) > 0) {
        if(buf[rdsz - 1] == '\n') --rdsz;
        buf[rdsz] = 0;  /* add null-terminator */
        __android_log_write(ANDROID_LOG_DEBUG, tag, buf);
    }
    return 0;
}

// call this function to redirect native library stderr output to android logcat
int start_logger(const char *app_name)
{
    tag = app_name;

    /* make stdout line-buffered and stderr unbuffered */
    setvbuf(stdout, 0, _IOLBF, 0);
    setvbuf(stderr, 0, _IONBF, 0);

    /* create the pipe and redirect stdout and stderr */
    pipe(pfd);
    dup2(pfd[1], 1);
    dup2(pfd[1], 2);

    /* spawn the logging thread */
    if(pthread_create(&thr, 0, thread_func, 0) != 0)
        return -1;

    pthread_detach(thr);
    return 0;
}

/* =========================================== */

/*
 * Method:    codec_ctx_malloc
 */
VPX_FUNC(jlong, codec_1ctx_1malloc)
{
    return (jlong) (intptr_t) malloc(sizeof(vpx_codec_ctx_t));
}

/*
 * Method:    codec_dec_init
 */
VPX_FUNC(jint, codec_1dec_1init, jlong context, jint iface, jlong cfg, jlong flags)
{
   LOGD("VPX decode using: %s\n", vpx_codec_iface_name(GET_INTERFACE(iface)));
   vpx_codec_err_t err = vpx_codec_dec_init(
                (vpx_codec_ctx_t *) (intptr_t) context,
                GET_INTERFACE(iface),
                (vpx_codec_dec_cfg_t *) (intptr_t) cfg,
                (vpx_codec_flags_t) flags);

    return (jint) err;
}

/*
 * Method:    codec_decode
 */
VPX_FUNC(jint, codec_1decode, jlong context,
     jbyteArray buf, jint buf_offset, jint buf_size, jlong user_priv, jlong deadline)
{
    jbyte *buf_ptr = (*env)->GetByteArrayElements(env, buf, NULL);

    vpx_codec_err_t status = vpx_codec_decode((vpx_codec_ctx_t *) (intptr_t) context,
                           (uint8_t *) (buf_ptr + buf_offset),
                           (unsigned int) buf_size,
                           (void *) (intptr_t) user_priv,
                           (long) deadline);

    (*env)->ReleaseByteArrayElements(env, buf, buf_ptr, JNI_ABORT);
    return (jint) status;
}

/*
 * Method:    codec_get_frame
 */
VPX_FUNC(jlong, codec_1get_1frame, jlong context, jlongArray iterArray)
{
    jlong *iter_ptr = (*env)->GetLongArrayElements(env, iterArray, NULL);

    vpx_image_t *ret;
    ret = vpx_codec_get_frame((vpx_codec_ctx_t *) (intptr_t) context,
                              (vpx_codec_iter_t *) iter_ptr);

    (*env)->ReleaseLongArrayElements(env, iterArray, iter_ptr, 0);
    return (jlong) (intptr_t) ret;
}

/*
 * Method:    codec_destroy
 */
VPX_FUNC(jint, codec_1destroy, jlong context)
{
    return (jint) vpx_codec_destroy((vpx_codec_ctx_t *) (intptr_t) context);
}

/*
 * Method:    codec_enc_init
 */
VPX_FUNC(jint, codec_1enc_1init, jlong context, jint iface, jlong cfg, jlong flags)
{
    LOGD("VPX encode using: %s\n", vpx_codec_iface_name(GET_INTERFACE(iface)));
    return (jint) vpx_codec_enc_init(
                        (vpx_codec_ctx_t *) (intptr_t) context,
                        GET_INTERFACE(iface),
                        (vpx_codec_enc_cfg_t *) (intptr_t) cfg,
                        (vpx_codec_flags_t) flags);
}

/*
 * Method:    codec_enc_config_set
 */
VPX_FUNC(jint, codec_1enc_1config_1set, jlong context, jlong cfg)
{
    return (jint) vpx_codec_enc_config_set(
                (vpx_codec_ctx_t *) (intptr_t) context,
                (vpx_codec_enc_cfg_t *) (intptr_t) cfg);
}

/*
 * Method:    codec_control
 */
VPX_FUNC(jint, codec_1control, jlong context, jint ctrl_id, jint arg)
{
    return (jint) vpx_codec_control_(
            (vpx_codec_ctx_t *) (intptr_t) context,
            (int) ctrl_id,
            (int) arg);
}

/*
 * Method:    codec_encode_frame => return the first encoded packet
 */
//VPX_FUNC(jlong, codec_1encode_1frame, jlong context, jlong jimg, jbyteArray bufArray, jint offsetY,
//         jint offsetU, jint offsetV, jlong pts, jlong duration, jlong flags, jlong deadline, jlongArray iterArray)
//{
//    vpx_codec_ctx_t *codec = (vpx_codec_ctx_t *) (intptr_t) context;
//
//    unsigned char *buf = (unsigned char *) (*env)->GetByteArrayElements(env, bufArray, NULL);
//    vpx_image_t *img = (vpx_image_t *) (intptr_t) jimg;
//    img->planes[VPX_PLANE_Y] = (buf + offsetY);
//    img->planes[VPX_PLANE_U] = (buf + offsetU);
//    img->planes[VPX_PLANE_V] = (buf + offsetV);
//    img->planes[VPX_PLANE_ALPHA] = 0;
//
//    const vpx_codec_err_t res = vpx_codec_encode(codec, img,
//                                                 (vpx_codec_pts_t) pts,
//                                                 (unsigned long) duration,
//                                                 (vpx_enc_frame_flags_t) flags,
//                                                 (unsigned long) deadline);
//
//    (*env)->ReleaseByteArrayElements(env, bufArray, (jbyte *)buf, JNI_ABORT);
//
//    const vpx_codec_cx_pkt_t *pkt = NULL;
//    if (res != VPX_CODEC_OK) {
//        return (jlong) (intptr_t) pkt;
//    }
//
//    jlong *iter_ptr = (*env)->GetLongArrayElements(env, iterArray, NULL);
//    pkt = vpx_codec_get_cx_data(codec, (vpx_codec_iter_t *) (intptr_t) iter_ptr);
//    // LOGD("VPX encoded packet: 0x%0X %u\n", *pkt, pkt->kind);
//
//    (*env)->ReleaseLongArrayElements(env, iterArray, iter_ptr, 0);
//    return (jlong) (intptr_t) pkt;
//}

/*
 * Method:    codec_encode
 */
VPX_FUNC(jint, codec_1encode, jlong context, jlong jimg, jbyteArray bufArray, jint offsetY,
         jint offsetU, jint offsetV, jlong pts, jlong duration, jlong flags, jlong deadline)
{
    unsigned char *buf = (unsigned char *) (*env)->GetByteArrayElements(env, bufArray, NULL);
    vpx_image_t *img = (vpx_image_t *) (intptr_t) jimg;
    img->planes[VPX_PLANE_Y] = (buf + offsetY);
    img->planes[VPX_PLANE_U] = (buf + offsetU);
    img->planes[VPX_PLANE_V] = (buf + offsetV);
    img->planes[VPX_PLANE_ALPHA] = 0;


    jint ret = (jint) vpx_codec_encode(
                    (vpx_codec_ctx_t *) (intptr_t) context,
                    img,
                    (vpx_codec_pts_t) pts,
                    (unsigned long) duration,
                    (vpx_enc_frame_flags_t) flags,
                    (unsigned long) deadline);

    (*env)->ReleaseByteArrayElements(env, bufArray, (jbyte *)buf, JNI_ABORT);
    return ret;
}

/*
 * Method:    codec_get_cx_data
 */
VPX_FUNC(jlong, codec_1get_1cx_1data, jlong context, jlongArray iterArray)
{
    jlong *iter_ptr = (*env)->GetLongArrayElements(env, iterArray, NULL);

    const vpx_codec_cx_pkt_t *pkt;
    pkt = vpx_codec_get_cx_data((vpx_codec_ctx_t *) (intptr_t) context,
                                (vpx_codec_iter_t *) iter_ptr);

    // LOGD("Packet pointer: %d\n", pkt);
    (*env)->ReleaseLongArrayElements(env, iterArray, iter_ptr, 0);
    return (jlong) (intptr_t) pkt;
}

/*
 * Method:    codec_cx_pkt_get_kind
 */
VPX_FUNC(jint, codec_1cx_1pkt_1get_1kind, jlong pkt)
{
    jint ret = (jint) ((vpx_codec_cx_pkt_t *) (intptr_t) pkt)->kind;
    return ret;
}

/*
 * Method:    codec_cx_pkt_get_size
 */
VPX_FUNC(jint, codec_1cx_1pkt_1get_1size, jlong pkt)
{
    return (jint) ((vpx_codec_cx_pkt_t *) (intptr_t) pkt)->data.frame.sz;
}

/*
 * Method:    codec_cx_pkt_get_data
 */
VPX_FUNC(jlong, codec_1cx_1pkt_1get_1data, jlong pkt)
{
      return (jlong) (intptr_t)
                    ((vpx_codec_cx_pkt_t *) (intptr_t) pkt)->data.frame.buf;
}

/*
 * Method:    img_malloc
 */
VPX_FUNC(jlong, img_1malloc)
{
    return (jlong) (intptr_t) malloc(sizeof(vpx_image_t));
}

/*
 * Method:    img_alloc with parameters
 */
VPX_FUNC(jlong, img_1alloc, jlong img, jint img_fmt, jint frame_width, jint frame_height, jint align)
{
    // Uncomment this for native library stderr message dump to logcat
    // start_logger(TAG);

    vpx_image_t *raw = (vpx_image_t *) (intptr_t) img;
    return (jlong) (intptr_t) vpx_img_alloc(raw, (int) img_fmt, (int) frame_width, (int) frame_height, (int) align);
}

/*
 * Method:    img_get_w
 */
VPX_FUNC(jint, img_1get_1w, jlong img)
{
    return (jint) ((vpx_image_t *) (intptr_t) img)->w;
}

/*
 * Method:    img_get_h
 */
VPX_FUNC(jint, img_1get_1h, jlong img)
{
    return (jint) ((vpx_image_t *) (intptr_t) img)->h;
}

/*
 * Method:    img_get_d_w
 */
VPX_FUNC(jint, img_1get_1d_1w, jlong img)
{
    return (jint) ((vpx_image_t *) (intptr_t) img)->d_w;
}

/*
 * Method:    img_get_d_h
 */
VPX_FUNC(jint, img_1get_1d_1h, jlong img)
{
    return (jint) ((vpx_image_t *) (intptr_t) img)->d_h;
}

/*
 * Method:    img_get_plane0
 */
VPX_FUNC(jlong, img_1get_1plane0, jlong img)
{
    return (jlong) (intptr_t) ((vpx_image_t *) (intptr_t) img)->planes[0];
}

/*
 * Method:    img_get_plane1
 */
VPX_FUNC(jlong, img_1get_1plane1, jlong img)
{
    return (jlong) (intptr_t) ((vpx_image_t *) (intptr_t) img)->planes[1];
}

/*
 * Method:    img_get_plane2
 */
VPX_FUNC(jlong, img_1get_1plane2, jlong img)
{
    return (jlong) (intptr_t) ((vpx_image_t *) (intptr_t) img)->planes[2];
}

/*
 * Method:    img_get_stride0
 */
VPX_FUNC(jint, img_1get_1stride0, jlong img)
{
    return (jint) ((vpx_image_t *) (intptr_t) img)->stride[0];
}

/*
 * Method:    img_get_stride1
 */
VPX_FUNC(jint, img_1get_1stride1, jlong img)
{
    return (jint) ((vpx_image_t *) (intptr_t) img)->stride[1];
}

/*
 * Method:    img_get_stride2
 */
VPX_FUNC(jint, img_1get_1stride2, jlong img)
{
    return (jint) ((vpx_image_t *) (intptr_t) img)->stride[2];
}

/*
 * Method:    img_get_fmt
 */
VPX_FUNC(jint, img_1get_1fmt, jlong img)
{
    return (jint) ((vpx_image_t *) (intptr_t) img)->fmt;
}

DEFINE_IMG_INT_PROPERTY_SETTER(w, w)
DEFINE_IMG_INT_PROPERTY_SETTER(h, h)
DEFINE_IMG_INT_PROPERTY_SETTER(d_1w, d_w)
DEFINE_IMG_INT_PROPERTY_SETTER(d_1h, d_h)
DEFINE_IMG_INT_PROPERTY_SETTER(stride0, stride[0])
DEFINE_IMG_INT_PROPERTY_SETTER(stride1, stride[1])
DEFINE_IMG_INT_PROPERTY_SETTER(stride2, stride[2])
DEFINE_IMG_INT_PROPERTY_SETTER(stride3, stride[3])
DEFINE_IMG_INT_PROPERTY_SETTER(fmt, fmt)
DEFINE_IMG_INT_PROPERTY_SETTER(bps, bps)

/*
 * Method:    img_wrap
 */
VPX_FUNC(void, img_1wrap, jlong img, jint fmt, jint d_w, jint d_h, jint align, jlong data)
{
    vpx_img_wrap((vpx_image_t *) (intptr_t) img,
                 (vpx_img_fmt_t) fmt,
                 (unsigned int) d_w,
                 (unsigned int) d_h,
                 (unsigned int) align,
                 (unsigned char *) (intptr_t) data);
}

/*
 * Method:    codec_dec_cfg_malloc
 */
VPX_FUNC(jlong, codec_1dec_1cfg_1malloc)
{
    return (jlong) (intptr_t) malloc(sizeof(vpx_codec_dec_cfg_t));
}

/*
 * Method:    codec_dec_cfg_set_w
 */
VPX_FUNC(void, codec_1dec_1cfg_1set_1w, jlong cfg, jint width)
{
    ((vpx_codec_dec_cfg_t *) (intptr_t) cfg)->w = width;
}

/*
 * Method:    codec_dec_cfg_set_w
 */
VPX_FUNC(void, codec_1dec_1cfg_1set_1h, jlong cfg, jint height)
{
    ((vpx_codec_dec_cfg_t *) (intptr_t) cfg)->h = height;
}

/*
 * Method:    codec_enc_cfg_malloc
 */
VPX_FUNC(jlong, codec_1enc_1cfg_1malloc) {
    return (jlong) (intptr_t) malloc(sizeof(vpx_codec_enc_cfg_t));
}

/*
 * Method:    codec_enc_config_default
 */
VPX_FUNC(jint, codec_1enc_1config_1default, jint iface, jlong cfg, jint usage)
{
    return vpx_codec_enc_config_default(
                     GET_INTERFACE(iface),
                     (vpx_codec_enc_cfg_t *) (intptr_t) cfg,
                     (int) usage);
}

DEFINE_ENC_CFG_INT_PROPERTY_SETTER(error_1resilient, g_error_resilient)
DEFINE_ENC_CFG_INT_PROPERTY_SETTER(lag_1in_1frames, g_lag_in_frames)
DEFINE_ENC_CFG_INT_PROPERTY_SETTER(profile, g_profile)
DEFINE_ENC_CFG_INT_PROPERTY_SETTER(threads, g_threads)
DEFINE_ENC_CFG_INT_PROPERTY_SETTER(w, g_w)
DEFINE_ENC_CFG_INT_PROPERTY_SETTER(h, g_h)

DEFINE_ENC_CFG_INT_PROPERTY_SETTER(tbnum, g_timebase.num)
DEFINE_ENC_CFG_INT_PROPERTY_SETTER(tbden, g_timebase.den)

DEFINE_ENC_CFG_INT_PROPERTY_SETTER(rc_1target_1bitrate, rc_target_bitrate)
DEFINE_ENC_CFG_INT_PROPERTY_SETTER(rc_1dropframe_1thresh, rc_dropframe_thresh)
DEFINE_ENC_CFG_INT_PROPERTY_SETTER(rc_1resize_1allowed, rc_resize_allowed)
DEFINE_ENC_CFG_INT_PROPERTY_SETTER(rc_1resize_1up_1thresh, rc_resize_up_thresh)
DEFINE_ENC_CFG_INT_PROPERTY_SETTER(rc_1resize_1down_1thresh, rc_resize_down_thresh)
DEFINE_ENC_CFG_INT_PROPERTY_SETTER(rc_1end_1usage, rc_end_usage)
DEFINE_ENC_CFG_INT_PROPERTY_SETTER(rc_1min_1quantizer, rc_min_quantizer)
DEFINE_ENC_CFG_INT_PROPERTY_SETTER(rc_1max_1quantizer, rc_max_quantizer)
DEFINE_ENC_CFG_INT_PROPERTY_SETTER(rc_1undershoot_1pct, rc_undershoot_pct)
DEFINE_ENC_CFG_INT_PROPERTY_SETTER(rc_1overshoot_1pct, rc_overshoot_pct)
DEFINE_ENC_CFG_INT_PROPERTY_SETTER(rc_1buf_1sz, rc_buf_sz)
DEFINE_ENC_CFG_INT_PROPERTY_SETTER(rc_1buf_1initial_1sz, rc_buf_initial_sz)
DEFINE_ENC_CFG_INT_PROPERTY_SETTER(rc_1buf_1optimal_1sz, rc_buf_optimal_sz)

DEFINE_ENC_CFG_INT_PROPERTY_SETTER(kf_1mode, kf_mode)
DEFINE_ENC_CFG_INT_PROPERTY_SETTER(kf_1min_1dist, kf_min_dist)
DEFINE_ENC_CFG_INT_PROPERTY_SETTER(kf_1max_1dist, kf_max_dist)

/*
 * Method:    stream_info_malloc
 */
VPX_FUNC(jlong, stream_1info_1malloc)
{
    return (jlong) (intptr_t) malloc(sizeof(vpx_codec_stream_info_t));
}

/*
 * Method:    stream_info_get_w
 */
VPX_FUNC(jint, stream_1info_1get_1w, jlong si)
{
    return (jint) ((vpx_codec_stream_info_t *) (intptr_t) si)->w;
}

/*
 * Method:    stream_info_get_h
 */
VPX_FUNC(jint, stream_1info_1get_1h, jlong si)
{
    return (jint) ((vpx_codec_stream_info_t *) (intptr_t) si)->h;
}

/*
 * Method:    stream_info_get_is_kf
 */
VPX_FUNC(jint, stream_1info_1get_1is_1kf, jlong si)
{
    return (jint) ((vpx_codec_stream_info_t *) (intptr_t) si)->h;
}

/*
 * Method:    codec_peek_stream_info
 */
VPX_FUNC(jint, codec_1peek_1stream_1info,  jint iface,
     jbyteArray buf, jint buf_offset, jint buf_size, jlong si)
{
    vpx_codec_err_t ret;
    jbyte *buf_ptr = (*env)->GetByteArrayElements(env, buf, NULL);

    ret = vpx_codec_peek_stream_info(GET_INTERFACE(iface),
                                     (uint8_t *) (buf_ptr + buf_offset),
                                     buf_size,
                                     (vpx_codec_stream_info_t *) (intptr_t)si);
    (*env)->ReleaseByteArrayElements(env, buf, buf_ptr, JNI_ABORT);
    return ret;
}

/*
 * Method:    malloc
 */
VPX_FUNC(jlong, malloc, jlong size)
{
    return (jlong) (intptr_t) malloc((size_t) size);
}

/*
 * Method:    free
 */
VPX_FUNC(void, free, jlong ptr)
{
    free((void *) (intptr_t) ptr);
}

/*
 * Method:    memcpy
 */
VPX_FUNC(void, memcpy, jbyteArray dstArray, jlong src, jint n)
{
    jbyte *dst = (*env)->GetByteArrayElements(env, dstArray, NULL);
    memcpy(dst, (char *) (intptr_t) src, n);
    (*env)->ReleaseByteArrayElements(env, dstArray, dst, 0);
}

/*
 * Method:    codec_err_to_string
 */
VPX_FUNC(jint, codec_1err_1to_1string,  jint err, jbyteArray buf, jint buf_size)
{
    const char *err_str = vpx_codec_err_to_string((vpx_codec_err_t) err);
    jbyte *buf_ptr = (*env)->GetByteArrayElements(env, buf, NULL);

    int i;
    for(i = 0; i < buf_size-1 && err_str[i] != '\0'; i++)
        buf_ptr[i] = (jbyte) err_str[i];
    buf_ptr[i] = (jbyte) '\0';

    (*env)->ReleaseByteArrayElements(env, buf, buf_ptr, 0);

    return i;
}