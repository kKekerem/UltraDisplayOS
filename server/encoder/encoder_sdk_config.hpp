#pragma once

#if defined(__has_include)
#  if __has_include(<nvEncodeAPI.h>)
#    define UD_HAS_NVENC_SDK 1
#  else
#    define UD_HAS_NVENC_SDK 0
#  endif

#  if __has_include(<core/Factory.h>) && __has_include(<core/Context.h>) && __has_include(<components/VideoEncoderVCE.h>) && __has_include(<components/VideoEncoderHEVC.h>) && __has_include(<components/VideoEncoderAV1.h>)
#    define UD_HAS_AMF_SDK 1
#  else
#    define UD_HAS_AMF_SDK 0
#  endif

#  if defined(UD_ENABLE_QSV_SDK) && UD_ENABLE_QSV_SDK && __has_include(<vpl/mfx.h>) && __has_include(<vpl/mfxvideo.h>)
#    define UD_HAS_QSV_SDK 1
#  else
#    define UD_HAS_QSV_SDK 0
#  endif
#else
#  define UD_HAS_NVENC_SDK 0
#  define UD_HAS_AMF_SDK 0
#  define UD_HAS_QSV_SDK 0
#endif
