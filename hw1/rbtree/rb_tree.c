#include <stddef.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
# include "rb_tree.h"

#define	RB_RED		0
#define	RB_BLACK	1
# define unlikely(x)	__builtin_expect(!!(x), 0)

#define __rb_parent(pc)    ((struct rb_node *)(pc & ~3))
#define __rb_color(pc)     ((pc) & 1)
#define __rb_is_black(pc)  __rb_color(pc)
#define __rb_is_red(pc)    (!__rb_color(pc))
#define rb_color(rb)       __rb_color((rb)->__rb_parent_color)
#define rb_is_red(rb)      __rb_is_red((rb)->__rb_parent_color)
#define rb_is_black(rb)    __rb_is_black((rb)->__rb_parent_color)


static inline void rb_set_parent(struct rb_node *rb, struct rb_node *p)
{
	rb->__rb_parent_color = rb_color(rb) | (unsigned long)p;
}

static inline void rb_set_black(struct rb_node *rb)
{
	rb->__rb_parent_color |= RB_BLACK;
}

static inline struct rb_node *rb_red_parent(struct rb_node *red)
{
	return (struct rb_node *)red->__rb_parent_color;
}

static inline void rb_set_parent_color(struct rb_node *rb,
				       struct rb_node *p, int color)
{
	rb->__rb_parent_color = (unsigned long)p | color;
}

static inline void
__rb_change_child(struct rb_node *old, struct rb_node *new,
		  struct rb_node *parent, struct rb_root *root)
{
	if (parent) {
		if (parent->rb_left == old)
			parent->rb_left = new;
		else
			parent->rb_right = new;
	} else
		root->rb_node = new;
}

static inline void
__rb_rotate_set_parents(struct rb_node *old, struct rb_node *new,
			struct rb_root *root, int color)
{
	struct rb_node *parent = rb_parent(old);
	new->__rb_parent_color = old->__rb_parent_color;
	rb_set_parent_color(old, new, color);
	__rb_change_child(old, new, parent, root);
}

static __always_inline void
__rb_insert(struct rb_node *node, struct rb_root *root,
	    void (*augment_rotate)(struct rb_node *old, struct rb_node *new))
{
	struct rb_node *parent = rb_red_parent(node), *gparent, *tmp;

	while (true) {
		/*
		 * Loop invariant: node is red.
		 */
		if (unlikely(!parent)) {
			/*
			 * The inserted node is root. Either this is the
			 * first node, or we recursed at Case 1 below and
			 * are no longer violating 4).
			 */
			rb_set_parent_color(node, NULL, RB_BLACK);
			break;
		}

		/*
		 * If there is a black parent, we are done.
		 * Otherwise, take some corrective action as,
		 * per 4), we don't want a red root or two
		 * consecutive red nodes.
		 */
		if(rb_is_black(parent))
			break;

		gparent = rb_red_parent(parent);

		tmp = gparent->rb_right;
		if (parent != tmp) {	/* parent == gparent->rb_left */
			if (tmp && rb_is_red(tmp)) {
				/*
				 * Case 1 - node's uncle is red (color flips).
				 *
				 *       G            g
				 *      / \          / \
				 *     p   u  -->   P   U
				 *    /            /
				 *   n            n
				 *
				 * However, since g's parent might be red, and
				 * 4) does not allow this, we need to recurse
				 * at g.
				 */
				rb_set_parent_color(tmp, gparent, RB_BLACK);
				rb_set_parent_color(parent, gparent, RB_BLACK);
				node = gparent;
				parent = rb_parent(node);
				rb_set_parent_color(node, parent, RB_RED);
				continue;
			}

			tmp = parent->rb_right;
			if (node == tmp) {
				/*
				 * Case 2 - node's uncle is black and node is
				 * the parent's right child (left rotate at parent).
				 *
				 *      G             G
				 *     / \           / \
				 *    p   U  -->    n   U
				 *     \           /
				 *      n         p
				 *
				 * This still leaves us in violation of 4), the
				 * continuation into Case 3 will fix that.
				 */
				tmp = node->rb_left;
				parent->rb_right = tmp;
				node->rb_left = parent;
				if (tmp)
					rb_set_parent_color(tmp, parent,
							    RB_BLACK);
				rb_set_parent_color(parent, node, RB_RED);
				augment_rotate(parent, node);
				parent = node;
				tmp = node->rb_right;
			}

			/*
			 * Case 3 - node's uncle is black and node is
			 * the parent's left child (right rotate at gparent).
			 *
			 *        G           P
			 *       / \         / \
			 *      p   U  -->  n   g
			 *     /                 \
			 *    n                   U
			 */
			gparent->rb_left = tmp; /* == parent->rb_right */
			parent->rb_right = gparent;
			if (tmp)
				rb_set_parent_color(tmp, gparent, RB_BLACK);
			__rb_rotate_set_parents(gparent, parent, root, RB_RED);
			augment_rotate(gparent, parent);
			break;
		} else {
			tmp = gparent->rb_left;
			if (tmp && rb_is_red(tmp)) {
				/* Case 1 - color flips */
				rb_set_parent_color(tmp, gparent, RB_BLACK);
				rb_set_parent_color(parent, gparent, RB_BLACK);
				node = gparent;
				parent = rb_parent(node);
				rb_set_parent_color(node, parent, RB_RED);
				continue;
			}

			tmp = parent->rb_left;
			if (node == tmp) {
				/* Case 2 - right rotate at parent */
				tmp = node->rb_right;
				parent->rb_left = tmp;
				node->rb_right = parent;
				if (tmp)
					rb_set_parent_color(tmp, parent,
							    RB_BLACK);
				rb_set_parent_color(parent, node, RB_RED);
				augment_rotate(parent, node);
				parent = node;
				tmp = node->rb_left;
			}
			/* Case 3 - left rotate at gparent */
			gparent->rb_right = tmp; /* == parent->rb_left */
			parent->rb_left = gparent;
			if (tmp)
				rb_set_parent_color(tmp, gparent, RB_BLACK);
			__rb_rotate_set_parents(gparent, parent, root, RB_RED);
			augment_rotate(gparent, parent);
			break;
		}
	}
}

static __inline__ void
____rb_erase_color(struct rb_node *parent, struct rb_root *root,
	void (*augment_rotate)(struct rb_node *old, struct rb_node *new))
{
	struct rb_node *node = NULL, *sibling, *tmp1, *tmp2;

	while (true) {
		sibling = parent->rb_right;
		if (node != sibling) {
			if (rb_is_red(sibling)) {
				tmp1 = sibling->rb_left;
				parent->rb_right = tmp1;
				sibling->rb_left = parent;
				rb_set_parent_color(tmp1, parent, RB_BLACK);
				__rb_rotate_set_parents(parent, sibling, root,
							RB_RED);
				augment_rotate(parent, sibling);
				sibling = tmp1;
			}
			tmp1 = sibling->rb_right;
			if (!tmp1 || rb_is_black(tmp1)) {
				tmp2 = sibling->rb_left;
				if (!tmp2 || rb_is_black(tmp2)) {
					rb_set_parent_color(sibling, parent,
							    RB_RED);
					if (rb_is_red(parent))
						rb_set_black(parent);
					else {
						node = parent;
						parent = rb_parent(node);
						if (parent)
							continue;
					}
					break;
				}
				tmp1 = tmp2->rb_right;
				sibling->rb_left = tmp1;
				tmp2->rb_right = sibling;
				parent->rb_right = tmp2;
				if (tmp1)
					rb_set_parent_color(tmp1, sibling,
							    RB_BLACK);
				augment_rotate(sibling, tmp2);
				tmp1 = sibling;
				sibling = tmp2;
			}
			tmp2 = sibling->rb_left;
			parent->rb_right = tmp2;
			sibling->rb_left = parent;
			rb_set_parent_color(tmp1, sibling, RB_BLACK);
			if (tmp2)
				rb_set_parent(tmp2, parent);
			__rb_rotate_set_parents(parent, sibling, root,
						RB_BLACK);
			augment_rotate(parent, sibling);
			break;
		} else {
			sibling = parent->rb_left;
			if (rb_is_red(sibling)) {
				tmp1 = sibling->rb_right;
				parent->rb_left = tmp1;
				sibling->rb_right = parent;
				rb_set_parent_color(tmp1, parent, RB_BLACK);
				__rb_rotate_set_parents(parent, sibling, root,
							RB_RED);
				augment_rotate(parent, sibling);
				sibling = tmp1;
			}
			tmp1 = sibling->rb_left;
			if (!tmp1 || rb_is_black(tmp1)) {
				tmp2 = sibling->rb_right;
				if (!tmp2 || rb_is_black(tmp2)) {
					rb_set_parent_color(sibling, parent,
							    RB_RED);
					if (rb_is_red(parent))
						rb_set_black(parent);
					else {
						node = parent;
						parent = rb_parent(node);
						if (parent)
							continue;
					}
					break;
				}
				tmp1 = tmp2->rb_left;
				sibling->rb_right = tmp1;
				tmp2->rb_left = sibling;
				parent->rb_left = tmp2;
				if (tmp1)
					rb_set_parent_color(tmp1, sibling,
							    RB_BLACK);
				augment_rotate(sibling, tmp2);
				tmp1 = sibling;
				sibling = tmp2;
			}
			tmp2 = sibling->rb_right;
			parent->rb_left = tmp2;
			sibling->rb_right = parent;
			rb_set_parent_color(tmp1, sibling, RB_BLACK);
			if (tmp2)
				rb_set_parent(tmp2, parent);
			__rb_rotate_set_parents(parent, sibling, root,
						RB_BLACK);
			augment_rotate(parent, sibling);
			break;
		}
	}
}

void __rb_erase_color(struct rb_node *parent, struct rb_root *root,
	void (*augment_rotate)(struct rb_node *old, struct rb_node *new))
{
	____rb_erase_color(parent, root, augment_rotate);
}


static inline void dummy_propagate(struct rb_node *node, struct rb_node *stop) {}
static inline void dummy_copy(struct rb_node *old, struct rb_node *new) {}
static inline void dummy_rotate(struct rb_node *old, struct rb_node *new) {}

struct rb_augment_callbacks {
	void (*propagate)(struct rb_node *node, struct rb_node *stop);
	void (*copy)(struct rb_node *old, struct rb_node *new);
	void (*rotate)(struct rb_node *old, struct rb_node *new);
};

static const struct rb_augment_callbacks dummy_callbacks = {
	.propagate = dummy_propagate,
	.copy = dummy_copy,
	.rotate = dummy_rotate
};

void rb_insert_color(struct rb_node *node, struct rb_root *root)
{
	__rb_insert(node, root, dummy_rotate);
}

static __inline__ struct rb_node *
__rb_erase_augmented(struct rb_node *node, struct rb_root *root,
		     const struct rb_augment_callbacks *augment)
{
	struct rb_node *child = node->rb_right;
	struct rb_node *tmp = node->rb_left;
	struct rb_node *parent, *rebalance;
	unsigned long pc;

	if (!tmp) {
		pc = node->__rb_parent_color;
		parent = __rb_parent(pc);
		__rb_change_child(node, child, parent, root);
		if (child) {
			child->__rb_parent_color = pc;
			rebalance = NULL;
		} else
			rebalance = __rb_is_black(pc) ? parent : NULL;
		tmp = parent;
	} else if (!child) {
		tmp->__rb_parent_color = pc = node->__rb_parent_color;
		parent = __rb_parent(pc);
		__rb_change_child(node, tmp, parent, root);
		rebalance = NULL;
		tmp = parent;
	} else {
		struct rb_node *successor = child, *child2;

		tmp = child->rb_left;
		if (!tmp) {
			parent = successor;
			child2 = successor->rb_right;

			augment->copy(node, successor);
		} else {
			do {
				parent = successor;
				successor = tmp;
				tmp = tmp->rb_left;
			} while (tmp);
			child2 = successor->rb_right;
			parent->rb_left = child2;
			successor->rb_right = child;
			rb_set_parent(child, successor);

			augment->copy(node, successor);
			augment->propagate(parent, successor);
		}

		tmp = node->rb_left;
		successor->rb_left = tmp;
		rb_set_parent(tmp, successor);

		pc = node->__rb_parent_color;
		tmp = __rb_parent(pc);
		__rb_change_child(node, successor, tmp, root);

		if (child2) {
			rb_set_parent_color(child2, parent, RB_BLACK);
			rebalance = NULL;
		} else {
			rebalance = rb_is_black(successor) ? parent : NULL;
		}
		successor->__rb_parent_color = pc;
		tmp = successor;
	}

	augment->propagate(tmp, NULL);
	return rebalance;
}

void rb_erase(struct rb_node *node, struct rb_root *root)
{
	struct rb_node *rebalance;
	rebalance = __rb_erase_augmented(node, root, &dummy_callbacks);
	if (rebalance)
		____rb_erase_color(rebalance, root, dummy_rotate);
}


void __rb_insert_augmented(struct rb_node *node, struct rb_root *root,
	void (*augment_rotate)(struct rb_node *old, struct rb_node *new))
{
	__rb_insert(node, root, augment_rotate);
}

struct rb_node *rb_first(const struct rb_root *root)
{
	struct rb_node	*n;

	n = root->rb_node;
	if (!n)
		return NULL;
	while (n->rb_left)
		n = n->rb_left;
	return n;
}

struct rb_node *rb_last(const struct rb_root *root)
{
	struct rb_node	*n;

	n = root->rb_node;
	if (!n)
		return NULL;
	while (n->rb_right)
		n = n->rb_right;
	return n;
}

struct rb_node *rb_next(const struct rb_node *node)
{
	struct rb_node *parent;

	if (RB_EMPTY_NODE(node))
		return NULL;

	if (node->rb_right) {
		node = node->rb_right;
		while (node->rb_left)
			node = node->rb_left;
		return (struct rb_node *)node;
	}

	while ((parent = rb_parent(node)) && node == parent->rb_right)
		node = parent;

	return parent;
}

struct rb_node *rb_prev(const struct rb_node *node)
{
	struct rb_node *parent;

	if (RB_EMPTY_NODE(node))
		return NULL;

	if (node->rb_left) {
		node = node->rb_left;
		while (node->rb_right)
			node = node->rb_right;
		return (struct rb_node *)node;
	}

	while ((parent = rb_parent(node)) && node == parent->rb_left)
		node = parent;

	return parent;
}

void rb_replace_node(struct rb_node *victim, struct rb_node *new,
		     struct rb_root *root)
{
	struct rb_node *parent = rb_parent(victim);

	*new = *victim;

	if (victim->rb_left)
		rb_set_parent(victim->rb_left, new);
	if (victim->rb_right)
		rb_set_parent(victim->rb_right, new);
	__rb_change_child(victim, new, parent, root);
}

static struct rb_node *rb_left_deepest_node(const struct rb_node *node)
{
	for (;;) {
		if (node->rb_left)
			node = node->rb_left;
		else if (node->rb_right)
			node = node->rb_right;
		else
			return (struct rb_node *)node;
	}
}

struct rb_node *rb_next_postorder(const struct rb_node *node)
{
	const struct rb_node *parent;
	if (!node)
		return NULL;
	parent = rb_parent(node);

	if (parent && node == parent->rb_left && parent->rb_right) {
		return rb_left_deepest_node(parent->rb_right);
	} else
		return (struct rb_node *)parent;
}

struct rb_node *rb_first_postorder(const struct rb_root *root)
{
	if (!root->rb_node)
		return NULL;

	return rb_left_deepest_node(root->rb_node);
}

/* Test program */
#define container_of(ptr, type, member) \
    ((type *) ((char *) (ptr) - (offsetof(type, member))))

#define treeint_entry(ptr) container_of(ptr, struct treeint, rb_n)

struct treeint {
    int value;
    struct rb_node rb_n;
};

static struct rb_root *tree;

int treeint_init()
{
    tree = calloc(sizeof(struct rb_root), 1);
    assert(tree);
    return 0;
}

static void __treeint_destroy(struct rb_node *n)
{
    if (left(n))
        __treeint_destroy(left(n));

    if (right(n))
        __treeint_destroy(right(n));

    struct treeint *i = treeint_entry(n);
    free(i);
}

int treeint_destroy()
{
    assert(tree);
    if (tree->rb_node)
        __treeint_destroy(tree->rb_node);

    free(tree);
    return 0;
}

struct treeint *treeint_insert(int a)
{
    struct rb_node *p = NULL;
    struct rb_node **n = &tree->rb_node;
    while (*n) {
        struct treeint *t = treeint_entry(*n);
        if (a == t->value)
            return NULL;
        p = *n;
        if (a < t->value) {
            n = &left((*n));
        } else if (a > t->value) {
            n = &right((*n));
        }
    }

    struct treeint *i = calloc(sizeof(struct treeint), 1);
    rb_link_node(&i->rb_n, p, n);
    rb_insert_color(&i->rb_n, tree);

    i->value = a;
    return i;
}

struct treeint *treeint_find(int a)
{
    struct rb_node *n = rb_root(tree);
    while (n) {
        struct treeint *t = treeint_entry(n);
        if (a == t->value)
            return t;

        if (a < t->value)
            n = left(n);
        else if (a > t->value)
            n = right(n);
    }

    return 0;
}

int treeint_remove(int a)
{
    struct treeint *n = treeint_find(a);
    if (!n)
        return -1;

    rb_erase(&n->rb_n, tree);
    free(n);
    return 0;
}

/* ascending order */
static void __treeint_dump(struct rb_node *n, int depth)
{
    if (!n)
        return;

    __treeint_dump(left(n), depth + 1);

    struct treeint *v = treeint_entry(n);
    printf("depth: %d    color:%ld     value:%d\n", depth, rb_color(&v->rb_n), v->value);

    __treeint_dump(right(n), depth + 1);
}

void treeint_dump()
{
    __treeint_dump(tree->rb_node, 0);
}

int main()
{
    srand(time(0));
    
    treeint_init();
    for (int i = 0; i < 100; ++i)
        treeint_insert(rand() % 99);

    printf("[ After insertions ]\n");
    treeint_dump();

	printf("Removing...\n");
    for (int i = 0; i < 100; ++i) {
        int v = rand() % 99;
        printf("%2d  ", v);
        if ((i + 1) % 10 == 0)
            printf("\n");
        treeint_remove(v);
    }
    printf("\n");

    printf("[ After removals ]\n");
    treeint_dump();

    treeint_destroy();

    return 0;
}