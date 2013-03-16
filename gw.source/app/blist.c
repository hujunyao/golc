#include "blist.h"

gw_blist_t* gw_blist_new() {
	gw_blist_t *blist = NULL;

	blist = calloc(1, sizeof(gw_blist_t));
	blist->sta = GW_BUDDY_STATUS_NONE;
	
	return blist;
}

void gw_blist_free(gw_blist_t *blist) {
	if(blist) {
		free(blist);
	}
}

int gw_blist_add_buddy(gw_blist_t *blist, gw_buddy_t *buddy) {
	int idx = 0;
	
	for(; idx < BLIST_MAXSIZE; idx++) {
		if(blist->buddies[idx].id == -1) {
			blist->buddies[idx] = *buddy;
			break;
		}
	}
	if(idx >= BLIST_MAXSIZE) {
		return -1;
	}

	blist->sta = GW_BUDDY_STATUS_UPDATE;
	blist->num ++;

	return idx;
}

int gw_blist_remove_buddy(gw_blist_t *blist ,int id) {
	int idx = 0;

	for(; idx < BLIST_MAXSIZE; idx++) {
		if(id == blist->buddies[idx].id) {
			memset(&(blist->buddies[idx]), 0, sizeof(gw_buddy_t));
			blist->buddies[idx].id = -1;
			break;
		}
	}

	if(idx >= BLIST_MAXSIZE) {
		return -1;
	}

	blist->sta = GW_BUDDY_STATUS_UPDATE;
	blist->num --;

	return idx;
}

int gw_blist_check_message(gw_blist_t *blist, gw_message_t *msg) {
	if(msg->from_id > 0) {
		for(idx = 0; idx < BLIST_MAXSIZE; idx++) {
			if(msg->from_id == blist->buddies[idx].id) {
				return 0;
			}
		}
	}

	return -1
}
