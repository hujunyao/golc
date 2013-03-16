#ifndef __BLIST_H__
#define __BLIST_H__

#define BUDDY_NAMESIZE 128

#define BLIST_MAXSIZE 12

typedef struct {
	int from_id, to_id;

	unsigned char *msgbuf;
} gw_message_t;

typedef enum {
	GW_BUDDY_TYPE_SERVER,
	GW_BUDDY_TYPE_APP
} gw_buddy_type_t;

typedef enum {
	GW_BUDDY_STATUS_NONE,
	GW_BUDDY_STATUS_UPDATE,
} gw_buddy_status_t;

typedef struct {
	gw_buddy_type_t type;
	char name[BUDDY_NAMESIZE];
	long id;
} gw_buddy_t;

typedef struct {
	int num;
	gw_buddy_t buddies[BLIST_MAXSIZE];
	gw_buddy_status_t sta;
} gw_blist_t;

gw_blist_t* gw_blist_new();
void gw_blist_free(gw_blist_t* blist);

int gw_blist_add_buddy(gw_blist_t *blist, gw_buddy_t *buddy);
int gw_blist_remove_buddy(gw_blist_t *blist, int id);

int gw_blist_check_message(gw_blist_t *blist, gw_message_t *msg);
#endif
