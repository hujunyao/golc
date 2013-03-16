#ifndef __GW_TYPES_H__
#define __GW_TYPES_H__

/* True iff e is an error that means a read/write operation can be retried. */
#define GW_ERR_RW_RETRIABLE(e)        \
  ((e) == EINTR || (e) == EAGAIN)
/* True iff e is an error that means an connect can be retried. */
#define GW_ERR_CONNECT_RETRIABLE(e)     \
  ((e) == EINTR || (e) == EINPROGRESS)
/* True iff e is an error that means a accept can be retried. */
#define GW_ERR_ACCEPT_RETRIABLE(e)      \
  ((e) == EINTR || (e) == EAGAIN || (e) == ECONNABORTED)

#endif
