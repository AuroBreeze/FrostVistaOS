#ifndef __LIST_H_
#define __LIST_H_

#define offsetof(type, member) ((uint64) & (((type *) 0)->member))
#define container_of(ptr, type, member)                                        \
	((type *) ((char *) ptr - offsetof(type, member)))

// assume new_node and head are (struct list_head *)
#define list_add_tail(new_node, head)                                          \
	do {                                                                   \
		struct list_head *_new = (new_node);                           \
		struct list_head *_head = (head);                              \
		_new->next = _head;                                            \
		_new->prev = _head->prev;                                      \
		_head->prev->next = _new;                                      \
		_head->prev = _new;                                            \
	} while (0)

// assume head is (struct list_head *)
#define list_is_empty(head) ((struct list_head *) (head)->next == (head))
#define list_first(head) ((struct list_head *) (head)->next)
#define list_del(head)                                                         \
	do {                                                                   \
		struct list_head *_head = (head);                              \
		struct list_head *_next = _head->next;                         \
		_head->prev->next = _next;                                     \
		_next->prev = _head->prev;                                     \
		_head->next = 0;                                               \
		_head->prev = 0;                                               \
	} while (0)

// assume pos and head are (struct list_head *)
#define list_for_each_entry(pos, head, type, member)                           \
	for (pos = container_of((head)->next, type, member);                   \
	     &pos->member != (head);                                           \
	     pos = container_of(pos->member.next, type, member))
#define list_for_each_safe(pos, n, head)                                       \
	for (pos = (head)->next, n = pos->next; pos != (head);                 \
	     pos = n, n = pos->next)

struct list_head {
	struct list_head *next;
	struct list_head *prev;
};

static inline void list_init(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}

#endif
