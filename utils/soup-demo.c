/**
  COMPILE: gcc -o demo soup-demo.c `pkg-config libsoup-2.4 --cflags --libs`
  TEST: ./demo username@gmail.com your_passwd
*/
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <glib.h>
#include <libsoup/soup.h>

#define DD(...) fprintf(stdout, __VA_ARGS__)

#define PM_AUTH_URL  "https://www.google.com/accounts/ClientLogin"
#define PM_SERVICE_NAME "lh2"
#define PM_AUTH_MIMETYPE "application/x-www-form-urlencoded"

static GMainLoop *loop;

static void session_cb(SoupSession *session, SoupMessage *msg, gpointer data) {
  GKeyFile *kf = NULL;
  gchar *auth_token;
  gchar *body = NULL;
  gint len = msg->response_body->length+strlen("[PM_AUTH]\n");

  kf = g_key_file_new();
  body = g_malloc0(len);
  g_sprintf(body, "[PM_AUTH]\n%s", msg->response_body->data);
  g_key_file_load_from_data(kf, body, len, G_KEY_FILE_NONE, NULL);
  auth_token = g_key_file_get_value(kf, "PM_AUTH", "Auth", NULL);
  g_key_file_free(kf);

  DD("auth_token = %s\n", auth_token);
  DD("status: %d\n", msg->status_code);
}

int main(int argc, char *argv[]) {
  SoupSession *session = NULL;
  SoupMessage *msg = NULL;
  gchar *body = NULL;

  //gtk_init(&argc, &argv);
  g_thread_init (NULL);
  g_type_init();
  session = soup_session_sync_new_with_options(SOUP_SESSION_USER_AGENT, "soup-demo", NULL);

  body = g_strdup_printf("accountType=HOSTED_OR_GOOGLE&Email=%s&Passwd=%s&service=%s&source=soup-demo", argv[1], argv[2], PM_SERVICE_NAME);
  DD("send auth message %s\n", body);
  msg = soup_message_new("POST", PM_AUTH_URL);
  soup_message_set_request(msg, PM_AUTH_MIMETYPE, SOUP_MEMORY_COPY, body, strlen(body));
  soup_session_queue_message(session, msg, session_cb, NULL);

  loop = g_main_loop_new (NULL, TRUE);
  g_main_loop_run (loop);
  g_main_loop_unref (loop);

  return 0;
}
