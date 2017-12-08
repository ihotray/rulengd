#define LN_LIST_HEAD(head, elem)								\
    struct head {                               \
        struct elem *first;                     \
    }

#define LN_LIST_NODE(elem)											\
    struct {                                    \
        struct elem *next;                      \
        struct elem **prev;                     \
    }

#define LN_LIST_HEAD_INITIALIZE(h)              \
    do {                                        \
        h.first = NULL;                         \
    } while (0)

#define LN_LIST_FOREACH(n, h, f)                          \
    for((n) = (h)->first; (n) != NULL; (n) = (n)->f.next)

#define LN_LIST_FOREACH_SAFE(n, h, f, t)																\
    for ((n) = (h)->first; (n) && ((t) = ((n)->f.next), 1); (n) = (t))

#define LN_LIST_INSERT(h, e, f)                 \
    do {                                        \
        (e)->f.next = (h)->first;               \
        if ((e)->f.next != NULL)                \
            (h)->first->f.prev = &(e)->f.next;  \
        (h)->first = (e);                       \
        (e)->f.prev = &(h)->first;              \
    } while (0)
