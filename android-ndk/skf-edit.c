#include <jni.h>
#include <time.h>
#include <android/log.h>
#include "flashengine/common/inc/mrc_base.h"

extern JNIEnv *g_env;
extern jobject g_obj;
static int bJNIEdit = 0;

#define SE_TAG "SKF-EDIT"
#define D(...) __android_log_print(ANDROID_LOG_ERROR, SE_TAG, __VA_ARGS__)

int32 mrc_editCreate(const char *title, const char *text, int32 type, int32 size) {
  if(bJNIEdit == 1)
    return MR_FAILED;

  jstring jstr;
  jclass clazz = (*g_env)->FindClass(g_env, "com/skymobi/flashplayer/FlashplayerActivity");
  if(clazz == 0) {
    D("Find Class failed");
    return MR_FAILED;
  }

  //D("g_env = %x, g_obj = %x\n", g_env, g_obj);
  //jboolean b = (* g_env)->IsInstanceOf(g_env, g_obj, clazz);
  jmethodID method = (*g_env)->GetMethodID(g_env, clazz, "startJNIEdit", "(Ljava/lang/String;)V");
  //D("STEP 01 , method = %x, g_env = %x, g_obj = %x, IsInstanceOf = %d\n", method, g_env, g_obj, b);
  if(method != NULL) {
    jstr = (*g_env)->NewStringUTF(g_env, text);
    (* g_env)->CallVoidMethod(g_env, g_obj, method, jstr);
    //bJNIEdit = 1;
    return MR_SUCCESS + 1;
  }
  return MR_FAILED;
}

int32 mrc_editRelease(int32 hEdit) {
  if(hEdit == MR_SUCCESS + 1) {
    bJNIEdit = 0;
    return MR_SUCCESS;
  }
}

const char *mrc_editGetText(int32 hEdit) {
  if(hEdit == MR_SUCCESS + 1) {
    jclass clazz = (*g_env)->FindClass(g_env, "com/skymobi/flashplayer/FlashplayerActivity");
    if(clazz == 0) {
      D("Find Class failed");
      return NULL;
    }

    jmethodID method = (*g_env)->GetMethodID(g_env, clazz, "getJNIEditString", "()Ljava/lang/String;");
    D("mrc_editGetText: method = %x, g_env = %x, g_obj = %x\n", method, g_env, g_obj);
    if(method != NULL) {
      jstring jstr = NULL;
      jstr = (jstring) (* g_env)->CallObjectMethod(g_env, g_obj, method);
      const jbyte *utf8 = (*g_env)->GetStringUTFChars(g_env, jstr, NULL);
      if(utf8) {
        D("mrc_editGetText = %s\n", utf8);
        (*g_env)->ReleaseStringUTFChars(g_env, jstr, utf8);
      }
      return NULL;
    }
  }
}
