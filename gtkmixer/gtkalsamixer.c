#include <gtk/gtk.h>
#include <alsa/asoundlib.h>

#define DEBUG(...) fprintf(stdout, "[alsamixer] "__VA_ARGS__)
#define DVM_CONFIG_PATH "./" //usr/lib/dvm-config"

const char *g_playback_iconset[] = {"rc/Audio_on.png", "rc/Audio_over.png", "rc/Audio_press.png",
                                    "rc/Audio_mute.png", "rc/Audio_mute_over.png", "rc/Audio_mute_press.png", NULL};
const char *g_capture_iconset [] = {"rc/Mic_on.png", "rc/Mic_over.png", "rc/Mic_press.png",
                                    "rc/Mic_mute.png", "rc/Mic_mute_over.png", "rc/Mic_mute_press.png", NULL};

typedef struct {
  snd_mixer_t *handle;
  snd_mixer_elem_t *master;
  snd_mixer_elem_t *pcm;
  snd_mixer_elem_t *mic;
  gboolean pcm_muted, mic_muted;
} alsa_mixer_t;

gboolean g_mixer_ui_updated = FALSE;
static gboolean alsa_mixer_is_muted(snd_mixer_elem_t *elem, gboolean *need_reset);

static int on_mixer_elem_event(snd_mixer_elem_t *elem, unsigned int mask) {
  snd_mixer_selem_id_t *sid;
  void *data = NULL;

  if(elem)
    data = snd_mixer_elem_get_callback_private(elem);

  if(data) {
    long min, max, Rvol, Lvol;

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_get_id(elem, sid);
    DEBUG("mixer elem control = %s, mask = %x, data = %x\n", snd_mixer_selem_id_get_name(sid), mask, data);
    if(mask & SND_CTL_EVENT_MASK_VALUE) {
      char path[1024] = {0};
      alsa_mixer_t *mixer = (alsa_mixer_t *)g_object_get_data(G_OBJECT(data), "mixer-obj");
      GtkWidget *img = (GtkWidget *)g_object_get_data(G_OBJECT(data), "image-obj");

      DEBUG("mixer = %x, img = %x\n", mixer, img);
      if(snd_mixer_selem_has_playback_volume(elem)) {
        if(mixer->pcm_muted != alsa_mixer_is_muted(elem, NULL)) {
          mixer->pcm_muted = !mixer->pcm_muted;
          g_snprintf(path, 1024, DVM_CONFIG_PATH"/%s", g_playback_iconset[mixer->pcm_muted ? 3: 0]);
          DEBUG("playback switch detected, image path = %s\n", path);
          gtk_image_set_from_file(GTK_IMAGE(img), path);
        }

        snd_mixer_selem_get_playback_volume(elem, 0, &Rvol);
        snd_mixer_selem_get_playback_volume(elem, 1, &Lvol);
        snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
        DEBUG("new val = %d, %d, min = %d, max = %d\n", Rvol, Lvol, min, max);
      } else if(snd_mixer_selem_has_capture_volume(elem)) {
        if(mixer->mic_muted != alsa_mixer_is_muted(elem, NULL)) {
          mixer->mic_muted = !mixer->mic_muted;
          g_snprintf(path, 1024, DVM_CONFIG_PATH"/%s", g_capture_iconset[mixer->mic_muted ? 3: 0]);
          DEBUG("capture switch detected, image path = %s\n", path);
          gtk_image_set_from_file(GTK_IMAGE(img), path);
        }

        snd_mixer_selem_get_capture_volume_range(elem, &min, &max);
        snd_mixer_selem_get_capture_volume(elem, 0, &Rvol);
        snd_mixer_selem_get_capture_volume(elem, 1, &Lvol);
        DEBUG("new val = %d, %d, min = %d, max = %d\n", Rvol, Lvol, min, max);
      }
      g_mixer_ui_updated = TRUE;
      gtk_adjustment_set_value(GTK_ADJUSTMENT(data), ((gdouble)(Rvol+Lvol)*50)/(max-min));
    }
  }

  return 0;
}

static int on_mixer_event(snd_mixer_t *handle, unsigned int mask, snd_mixer_elem_t *elem) {
  snd_mixer_selem_id_t *sid;

  if(mask & SND_CTL_EVENT_MASK_ADD) {
    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_get_id(elem, sid);
    DEBUG("set callback for mixer control = %s\n", snd_mixer_selem_id_get_name(sid));
    snd_mixer_elem_set_callback(elem, on_mixer_elem_event);
  }

  return 0;
}

static int alsa_mixer_init(alsa_mixer_t *mixer, char *card) {
  snd_mixer_selem_id_t *sid;
  snd_mixer_elem_t *elem;
  snd_mixer_selem_id_alloca(&sid);
  int err;

  if ((err = snd_mixer_open(&mixer->handle, 0)) < 0) {
    DEBUG("Mixer %s open error: %s", card, snd_strerror(err));
    mixer->handle = NULL;
    return err;
  }

  if ((err = snd_mixer_attach(mixer->handle, card)) < 0) {
    DEBUG("Mixer %s local error: %s\n", card, snd_strerror(err));
    snd_mixer_close(mixer->handle);
    mixer->handle = NULL;
    return err;
  }

  if ((err = snd_mixer_selem_register(mixer->handle, NULL, NULL)) < 0) {
    DEBUG("Mixer register error: %s", snd_strerror(err));
    snd_mixer_close(mixer->handle);
    return err;
  }

  snd_mixer_set_callback(mixer->handle, on_mixer_event);
  err = snd_mixer_load(mixer->handle);
  if (err < 0) {
    DEBUG("Mixer %s load error: %s", card, snd_strerror(err));
    snd_mixer_close(mixer->handle);
    return err;
  }

  for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) {
    const char *name = NULL;
    snd_mixer_selem_get_id(elem, sid);
    if (!snd_mixer_selem_is_active(elem))
      continue;

    name = snd_mixer_selem_id_get_name(sid);
    if(snd_mixer_selem_has_playback_volume(elem)) {
      if(strcmp(name, "Master") == 0)
        mixer->master = elem;
      else if(strcmp(name, "PCM") == 0)
        mixer->pcm = elem;
    } else if(snd_mixer_selem_has_capture_volume(elem) || snd_mixer_selem_has_capture_switch(elem)) {
      DEBUG("capture elem name = %s\n", name);
      /**make sure have one capture source if capture is available*/
      if(NULL == mixer->mic) {
        mixer->mic = elem;
      }
      /**if have Microphone, replace it*/
      if(strcmp(name, "Microphone") == 0)
        mixer->mic = elem;
      if(strcmp(name, "Capture") == 0)
        mixer->mic = elem;
    }
  }

  DEBUG("master = %x, pcm = %x, microphone = %x\n", mixer->master, mixer->pcm, mixer->mic);
  return 0;
}

static gboolean on_mixer_channel_event(GIOChannel *source, GIOCondition cond, gpointer data) {
  alsa_mixer_t *mixer = (alsa_mixer_t *)data;

  if(mixer->handle)
    snd_mixer_handle_events(mixer->handle);
  
  return TRUE;
}

static void alsa_mixer_add_io_watch(alsa_mixer_t *mixer) {
  struct pollfd fdset[16];
  int cnt, err, idx;
  GIOChannel *chan = NULL;

  cnt = snd_mixer_poll_descriptors(mixer->handle, fdset, sizeof(fdset)/sizeof(fdset[0]));
  if(cnt > 0) {
    DEBUG("detected %d fdset in alsa event poll.\n", cnt);
    for(idx = 0; idx < cnt; idx++) {
      chan = g_io_channel_unix_new(fdset[idx].fd);
      g_io_add_watch(chan, fdset[idx].events, (GIOFunc)on_mixer_channel_event, mixer);
    }
  }
}

static void alsa_mixer_term(alsa_mixer_t *mixer) {
  if(mixer->handle)
    snd_mixer_close(mixer->handle);
}

static gboolean alsa_mixer_is_muted(snd_mixer_elem_t *elem, gboolean *need_reset) {
  int sw;
  long Rvol, Lvol, min, max;

  if(need_reset)
    *need_reset = FALSE;

  /**if switch is available, get switch status directly*/
  if(snd_mixer_selem_has_playback_switch(elem)) {
    snd_mixer_selem_get_playback_switch(elem, 0, &sw);
    return sw ? FALSE: TRUE;
  } else if(snd_mixer_selem_has_capture_switch(elem)) {
    snd_mixer_selem_get_capture_switch(elem, 0, &sw);
    return sw ? FALSE: TRUE;
  }

  if(snd_mixer_selem_has_playback_volume(elem)) {
    snd_mixer_selem_get_playback_volume(elem, 0, &Rvol);
    snd_mixer_selem_get_playback_volume(elem, 1, &Lvol);
    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    if(need_reset)
      *need_reset = !!(max - Rvol);
    return (Rvol + Lvol) ? FALSE: TRUE;
  } else if(snd_mixer_selem_has_capture_volume(elem)) {
    snd_mixer_selem_get_capture_volume_range(elem, &min, &max);
    snd_mixer_selem_get_capture_volume(elem, 0, &Rvol);
    snd_mixer_selem_get_capture_volume(elem, 1, &Lvol);
    if(need_reset)
      *need_reset = !!(max - Rvol);
    return (Rvol + Lvol) ? FALSE: TRUE;
  }
}

static int alsa_mixer_set_muted(snd_mixer_elem_t *elem, gboolean muted) {
  long val, min, max;

  /**if switch is available, get switch status directly*/
  if(snd_mixer_selem_has_playback_switch(elem)) {
    snd_mixer_selem_set_playback_switch(elem, 0, !muted);
    snd_mixer_selem_set_playback_switch(elem, 1, !muted);
    return 0;
  } else if(snd_mixer_selem_has_capture_switch(elem)) {
    snd_mixer_selem_set_capture_switch(elem, 0, !muted);
    snd_mixer_selem_set_capture_switch(elem, 1, !muted);
    return 0;
  }

  if(snd_mixer_selem_has_playback_volume(elem)) {
    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    val = muted ? min : max;
    snd_mixer_selem_set_playback_volume(elem, 0, val);
    snd_mixer_selem_set_playback_volume(elem, 1, val);
    return 0;
  } else if(snd_mixer_selem_has_capture_volume(elem)) {
    snd_mixer_selem_get_capture_volume_range(elem, &min, &max);
    val = muted ? min : max;
    snd_mixer_selem_set_capture_volume(elem, 0, val);
    snd_mixer_selem_set_capture_volume(elem, 1, val);
    return 0;
  }

  return -1;
}

static void on_my_capture_mixer_changed (GtkRange *adj, gpointer data) {
  DEBUG("on_my_capture_mixer_changed \n");
}

static GtkWidget *am_create_mixer_ui (GtkWidget *switch_btn, char *name, GtkAdjustment *adj, GtkWidget *adv_btn) {
  GtkWidget *vbox = NULL, *label = NULL, *img = NULL, *scale = NULL, *vhbox = NULL;
  GtkWidget *spin = NULL;

  vbox = gtk_vbox_new(FALSE, 2);
  vhbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vhbox), switch_btn, TRUE, TRUE, 30);
  gtk_box_pack_start(GTK_BOX(vbox), vhbox, FALSE, TRUE, 0);

  label = gtk_label_new(name);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 8);

  scale = gtk_vscale_new(adj);
  gtk_widget_set_size_request(scale, -1, 200);
  gtk_range_set_inverted(GTK_RANGE(scale), TRUE);
  gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
  g_object_set(G_OBJECT(scale), "can-focus", FALSE, NULL);
  g_signal_connect(scale, "value-changed", G_CALLBACK(on_my_capture_mixer_changed), NULL);
  gtk_box_pack_start(GTK_BOX(vbox), scale, FALSE, TRUE, 0);

  vhbox = gtk_hbox_new(FALSE, 0);
  spin = gtk_spin_button_new(adj, 1, 0);
  gtk_box_pack_start(GTK_BOX(vhbox), spin, TRUE, TRUE, 24);
  gtk_box_pack_start(GTK_BOX(vbox), vhbox, FALSE, TRUE, 2);

  if(adv_btn)
    gtk_box_pack_start(GTK_BOX(vbox), adv_btn, FALSE, TRUE, 2);

  return vbox;
}

static void on_capture_mixer_changed (GtkAdjustment *adj, gpointer data) {
  alsa_mixer_t *mixer = (alsa_mixer_t *)data;
  long min, max, val;
  gdouble newval = gtk_adjustment_get_value(adj);

  if(g_mixer_ui_updated == FALSE && mixer->mic) {
    DEBUG("microphone mixer value changed\n");
    snd_mixer_selem_get_capture_volume_range(mixer->mic, &min, &max);
    val = ((newval/100) * (max-min)+0.5);
    snd_mixer_selem_set_capture_volume(mixer->mic, 0, val);
    snd_mixer_selem_set_capture_volume(mixer->mic, 1, val);
  }

  DEBUG("mixer->mic_muted = %d\n", mixer->mic_muted);
  if(g_mixer_ui_updated) g_mixer_ui_updated = !g_mixer_ui_updated;
}

static void on_playback_mixer_changed (GtkAdjustment *adj, gpointer data) {
  alsa_mixer_t *mixer = (alsa_mixer_t *)data;
  long min, max, val;
  gdouble newval = gtk_adjustment_get_value(adj);

  if(g_mixer_ui_updated == FALSE && mixer->pcm) {
    DEBUG("playback mixer value changed\n");
    snd_mixer_selem_get_playback_volume_range(mixer->pcm, &min, &max);
    val = ((newval/100) * (max-min) + 0.5);
    snd_mixer_selem_set_playback_volume(mixer->pcm, 0, val);
    snd_mixer_selem_set_playback_volume(mixer->pcm, 1, val);
  }

  DEBUG("mixer->pcm_muted = %d\n", mixer->pcm_muted);
  if(g_mixer_ui_updated) g_mixer_ui_updated = !g_mixer_ui_updated;
}

static gboolean on_enter_switch_widget (GtkWidget *widget, GdkEventCrossing *event, gpointer data) {
  char **iconset = g_object_get_data(G_OBJECT(widget), "iconset");
  alsa_mixer_t *mixer = (alsa_mixer_t *)data;
  gboolean muted, need_reset;
  snd_mixer_elem_t *elem = NULL;
  GtkWidget *img = NULL;
  char path[1024] = {0};

  img = gtk_bin_get_child(GTK_BIN(widget));
  if(iconset == (char **)g_playback_iconset) {
    elem = mixer->master;
    DEBUG("enter playback switch widget.\n");
  } else {
    elem = mixer->mic;
    DEBUG("enter capture switch widget.\n");
  }

  if(elem) {
    muted = alsa_mixer_is_muted(elem, &need_reset);

    g_snprintf(path, 1024, DVM_CONFIG_PATH"/%s", iconset[muted ? 4: 1]);
    gtk_image_set_from_file(GTK_IMAGE(img), path);
  }

  return TRUE;
}

static gboolean on_leave_switch_widget (GtkWidget *widget, GdkEventCrossing *event, gpointer data) {
  char **iconset = g_object_get_data(G_OBJECT(widget), "iconset");
  alsa_mixer_t *mixer = (alsa_mixer_t *)data;
  gboolean muted, need_reset;
  snd_mixer_elem_t *elem = NULL;
  GtkWidget *img = NULL;
  char path[1024] = {0};

  img = gtk_bin_get_child(GTK_BIN(widget));
  if(iconset == (char **)g_playback_iconset) {
    DEBUG("leave playback switch widget.\n");
    elem = mixer->master;
  } else {
    DEBUG("leave capture switch widget.\n");
    elem = mixer->mic;
  }

  if(elem) {
    muted = alsa_mixer_is_muted(elem, &need_reset);

    g_snprintf(path, 1024, DVM_CONFIG_PATH"/%s", iconset[muted ? 3: 0]);
    gtk_image_set_from_file(GTK_IMAGE(img), path);
  }
  return TRUE;
}

static gboolean on_press_switch_widget (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  char **iconset = g_object_get_data(G_OBJECT(widget), "iconset");
  alsa_mixer_t *mixer = (alsa_mixer_t *)data;
  gboolean muted, need_reset;
  snd_mixer_elem_t *elem = NULL;
  GtkWidget *img = NULL;
  char path[1024] = {0};

  img = gtk_bin_get_child(GTK_BIN(widget));
  if(iconset == (char **)g_playback_iconset) {
    DEBUG("press playback switch widget.\n");
    elem = mixer->master;
  } else {
    DEBUG("press capture switch widget.\n");
    elem = mixer->mic;
  }

  if(elem) {
    muted = alsa_mixer_is_muted(elem, &need_reset);

    g_snprintf(path, 1024, DVM_CONFIG_PATH"/%s", iconset[muted ? 5: 2]);
    gtk_image_set_from_file(GTK_IMAGE(img), path);
  }
  return TRUE;
}

static gboolean on_release_switch_widget (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  char **iconset = g_object_get_data(G_OBJECT(widget), "iconset");
  alsa_mixer_t *mixer = (alsa_mixer_t *)data;
  gboolean muted, need_reset, *pmuted = NULL;
  snd_mixer_elem_t *elem = NULL;
  GtkWidget *img = NULL;
  char path[1024] = {0};

  img = gtk_bin_get_child(GTK_BIN(widget));
  if(iconset == (char **)g_playback_iconset) {
    DEBUG("release playback switch widget.\n");
    elem = mixer->master;
    pmuted = &mixer->pcm_muted;
  } else {
    DEBUG("release capture switch widget.\n");
    elem = mixer->mic;
    pmuted = &mixer->mic_muted;
  }

  if(elem) {
    muted = alsa_mixer_is_muted(elem, &need_reset);
    DEBUG("muted = %d\n", !muted);
    alsa_mixer_set_muted(elem, !muted);
    muted = alsa_mixer_is_muted(elem, &need_reset);
    *pmuted = muted;

    g_snprintf(path, 1024, DVM_CONFIG_PATH"/%s", iconset[muted ? 3: 0]);
    gtk_image_set_from_file(GTK_IMAGE(img), path);
  }

  return TRUE;
}

static void am_create_playback_mixer(GtkWidget *hbox, alsa_mixer_t *mixer) {
  GtkWidget *switch_btn = NULL, *vbox = NULL, *img = NULL, *vvbox = NULL;
  GtkObject *adj = NULL;
  char *icon = NULL;
  gdouble dval = 0;
  long Rvol, Lvol, pmin, pmax;
  gboolean muted = TRUE, need_reset;
  char path[1024] = {0};

  if(mixer->master)
    muted = alsa_mixer_is_muted(mixer->master, &need_reset);
  mixer->pcm_muted = muted;

  g_snprintf(path, 1024, DVM_CONFIG_PATH"/%s", g_playback_iconset[muted ? 3: 0]);

  switch_btn = gtk_event_box_new();
  img = gtk_image_new_from_file(path);
  gtk_container_add(GTK_CONTAINER(switch_btn), img);
  g_object_set_data(G_OBJECT(switch_btn), "iconset", g_playback_iconset);
  g_signal_connect(switch_btn, "enter-notify-event", G_CALLBACK(on_enter_switch_widget), mixer);
  g_signal_connect(switch_btn, "leave-notify-event", G_CALLBACK(on_leave_switch_widget), mixer);
  g_signal_connect(switch_btn, "button-press-event", G_CALLBACK(on_press_switch_widget), mixer);
  g_signal_connect(switch_btn, "button-release-event", G_CALLBACK(on_release_switch_widget), mixer);

  if(mixer->pcm) {
    snd_mixer_selem_get_playback_volume(mixer->pcm, 0, &Rvol);
    snd_mixer_selem_get_playback_volume(mixer->pcm, 1, &Lvol);
    snd_mixer_selem_get_playback_volume_range(mixer->pcm, &pmin, &pmax);

    dval = ((gdouble)(Rvol + Lvol)*50)/(pmax-pmin);
    DEBUG("Rvol = %d, Lvol = %d, pmin = %d, pmax = %d, dval = %f\n", Rvol, Lvol, pmin, pmax, dval);
  }
  adj = gtk_adjustment_new(dval, 0, 100, 1, 6, 0);
  g_object_set_data(G_OBJECT(adj), "image-obj", img);
  g_object_set_data(G_OBJECT(adj), "mixer-obj", mixer);
  if(mixer->pcm) {
    snd_mixer_elem_set_callback_private(mixer->pcm, adj);
    g_signal_connect(adj, "value-changed", G_CALLBACK(on_playback_mixer_changed), mixer);
  }

  vbox = am_create_mixer_ui(switch_btn, "Main", GTK_ADJUSTMENT(adj), NULL);

  gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
}

static void am_create_capture_mixer(GtkWidget *hbox, alsa_mixer_t *mixer) {
  GtkWidget *switch_btn = NULL, *vbox = NULL, *adv_btn = NULL, *img = NULL;
  GtkObject *adj = NULL;
  char *icon = NULL;
  gdouble dval = 0;
  long cmin, cmax, Rvol, Lvol;
  gboolean muted = TRUE, need_reset;
  char path[1024] = {0};

  if(mixer->mic)
    muted = alsa_mixer_is_muted(mixer->mic, &need_reset);
  mixer->mic_muted = muted;

  g_snprintf(path, 1024, DVM_CONFIG_PATH"/%s", g_capture_iconset[muted ? 3: 0]);

  switch_btn = gtk_event_box_new();
  img = gtk_image_new_from_file(path);
  gtk_container_add(GTK_CONTAINER(switch_btn), img);
  g_object_set_data(G_OBJECT(switch_btn), "iconset", g_capture_iconset);
  g_signal_connect(switch_btn, "enter-notify-event", G_CALLBACK(on_enter_switch_widget), mixer);
  g_signal_connect(switch_btn, "leave-notify-event", G_CALLBACK(on_leave_switch_widget), mixer);
  g_signal_connect(switch_btn, "button-press-event", G_CALLBACK(on_press_switch_widget), mixer);
  g_signal_connect(switch_btn, "button-release-event", G_CALLBACK(on_release_switch_widget), mixer);

  if(mixer->mic) {
    snd_mixer_selem_get_capture_volume_range(mixer->mic, &cmin, &cmax);
    snd_mixer_selem_get_capture_volume(mixer->mic, 0, &Rvol);
    snd_mixer_selem_get_capture_volume(mixer->mic, 1, &Lvol);
    dval = ((gdouble)(Rvol + Lvol)*50)/(cmax-cmin);
    adj = gtk_adjustment_new(dval, 0, 100, 1, 6, 0);
    snd_mixer_elem_set_callback_private(mixer->mic, adj);
  } else {
    adj = gtk_adjustment_new(0, 0, 100, 1, 6, 0);
  }

  g_object_set_data(G_OBJECT(adj), "image-obj", img);
  g_object_set_data(G_OBJECT(adj), "mixer-obj", mixer);
  DEBUG("capture set object data, img = %x, mixer = %x\n", img, mixer);

  //g_signal_connect(adj, "value-changed", G_CALLBACK(on_capture_mixer_changed), mixer);

  adv_btn = gtk_button_new_with_label("Advanced");

  vbox = am_create_mixer_ui(switch_btn, "Microphone", GTK_ADJUSTMENT(adj), adv_btn);
  gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
}

static gboolean close_main_window(GtkWidget *widget, GdkEvent *event, gpointer data) {
  gtk_main_quit();

  return TRUE;
}

int main(int argc, char *argv[]) {
  GtkWidget *window = NULL, *hbox = NULL;
  alsa_mixer_t mixer = {0};
  char *card = NULL, *str = NULL;

  card = getenv("ALSA_CARD");
  if(card == NULL || strlen(card) <= 0)
    card = "default";
    
  if(alsa_mixer_init(&mixer, card) < 0) {
    DEBUG("alsa mixer init failure!\nPlease make sure export ALSA_CARD env correctly.");
    return -1;
  }

  gtk_init(&argc, &argv);

  hbox = gtk_hbox_new(FALSE, 0);
  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "AlSA Audio Mixer");
  gtk_container_set_border_width(GTK_CONTAINER(window), 12);
  g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(close_main_window), NULL);
  gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
  gtk_container_add(GTK_CONTAINER(window), hbox);

  alsa_mixer_add_io_watch(&mixer);

  am_create_playback_mixer(hbox, &mixer);
  str = getenv("GTK_ALSA_MIXER_DISABLE_MIC");
  if(!str || strcmp(str, "yes") != 0) {
    gtk_box_pack_start(GTK_BOX(hbox), gtk_vseparator_new(), FALSE, TRUE, 12);
    am_create_capture_mixer(hbox, &mixer);
  }

  gtk_widget_show_all(window);

  gtk_main();

  alsa_mixer_term(&mixer);

  return 0;
}
