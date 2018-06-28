// test_ziplist.cpp : 定义控制台应用程序的入口点。
//

#include <stdlib.h>
#include <string.h>
#include <assert.h>


typedef struct listNode {
	struct listNode *prev;
	struct listNode *next;
	void *value;
} listNode;


typedef struct listIter {
	listNode *next;
	int direction;
} listIter;


typedef struct list { // 在c语言中,用结构体的方式来模拟对象是一种常见的手法
	listNode *head;
	listNode *tail;
	void *(*dup)(void *ptr);
	void(*free)(void *ptr);
	int(*match)(void *ptr, void *key);
	unsigned long len;
} list;

/* Functions implemented as macros */

// listLength 返回给定链表所包含的节点数量
// T = O(1)
#define listLength(l) ((l)->len)

// listFirst 返回给定链表的表头节点
// T = O(1)
#define listFirst(l) ((l)->head)

// listLast 返回给定链表的表尾节点
// T = O(1)
#define listLast(l) ((l)->tail)

// listPrevNode 返回给定节点的前置节点
// T = O(1)
#define listPrevNode(n) ((n)->prev)

// listNextNode 返回给定节点的后置节点
// T = O(1)
#define listNextNode(n) ((n)->next)

// listNodeValue 返回给定节点的值
// T = O(1)
#define listNodeValue(n) ((n)->value)

// listSetDupMethod 将链表 l 的值复制函数设置为 m
// T = O(1)
#define listSetDupMethod(l,m) ((l)->dup = (m))

// listSetFreeMethod 将链表 l 的值释放函数设置为 m
// T = O(1)
#define listSetFreeMethod(l,m) ((l)->free = (m))

// listSetMatchMethod 将链表的对比函数设置为 m
// T = O(1)
#define listSetMatchMethod(l,m) ((l)->match = (m))

// listGetDupMethod 返回给定链表的值复制函数
// T = O(1) 
#define listGetDupMethod(l) ((l)->dup)

// listGetFree返回给定链表的值释放函数
// T = O(1)
#define listGetFree(l) ((l)->free)

// listGetMatchMethod 返回给定链表的值对比函数
// T = O(1)
#define listGetMatchMethod(l) ((l)->match)

/* Prototypes */
list *listCreate(void);
void listRelease(list *list);
list *listAddNodeHead(list *list, void *value);
list *listAddNodeTail(list *list, void *value);
list *listInsertNode(list *list, listNode *old_node, void *value, int after);
void listDelNode(list *list, listNode *node);
listIter *listGetIterator(list *list, int direction);
listNode *listNext(listIter *iter);
void listReleaseIterator(listIter *iter);
list *listDup(list *orig);
listNode *listSearchKey(list *list, void *key);
listNode *listIndex(list *list, long index);
void listRewind(list *list, listIter *li);
void listRewindTail(list *list, listIter *li);
void listRotate(list *list);

/* Directions for iterators -- 迭代器进行迭代的方向 */
#define AL_START_HEAD 0
#define AL_START_TAIL 

#define  zmalloc malloc
#define  zfree free

list *listCreate(void)
{
	struct list *li;
	if ((li = (list*)zmalloc(sizeof(*li))) == NULL)
		return NULL;
	li->head = li->tail = NULL;
	li->len = 0;
	li->dup = NULL;
	li->free = NULL;
	li->match = NULL;
	return li;
}


/*
* listRelease 释放整个链表，以及链表中所有节点, 这个函数不可能会失败.
*
* T = O(N)
*/
void listRelease(list *list)
{
	unsigned long len;
	listNode *current, *next;
	current = list->head;
	len = list->len;
	while (len--) {
		next = current->next;
		if (list->free) list->free(current->value);
		zfree(current);
		current = next;
	}
	zfree(list);
}


/*
* listAddNodeHead 将一个包含有给定值指针 value 的新节点添加到链表的表头
*
* 如果为新节点分配内存出错，那么不执行任何动作，仅返回 NULL
*
* 如果执行成功，返回传入的链表指针
*
* T = O(1)
*/
list *listAddNodeHead(list *list, void *value)
{
	listNode *node;
	if ((node = (listNode *)zmalloc(sizeof(*node))) == NULL)
		return NULL;
	node->value = value;

	if (list->len == 0) {
		list->head = list->tail = node;
		node->prev = node->next = NULL;
	}
	else { // 添加节点到非空链表
		node->prev = NULL;
		node->next = list->head;
		list->head->prev = node;
		list->head = node;
	}
	list->len++;
	return list;
}


/*
* listAddNodeTail 将一个包含有给定值指针 value 的新节点添加到链表的表尾
*
* 如果为新节点分配内存出错，那么不执行任何动作，仅返回 NULL
*
* 如果执行成功，返回传入的链表指针
*
* T = O(1)
*/
list *listAddNodeTail(list *list, void *value)
{
	listNode *node;
	if ((node = (listNode*)zmalloc(sizeof(*node))) == NULL)
		return NULL;

	node->value = value;
	if (list->len == 0) {
		list->head = list->tail = node;
		node->prev = node->next = NULL;
	}
	else {
		node->prev = list->tail;
		node->next = NULL;
		list->tail->next = node;
		list->tail = node;
	}
	list->len++;
	return list;
}

/*
* listInsertNode 创建一个包含值 value 的新节点，并将它插入到 old_node 的之前或之后
*
* 如果 after 为 0 ，将新节点插入到 old_node 之前。
* 如果 after 为 1 ，将新节点插入到 old_node 之后。
*
* T = O(1)
*/
list *listInsertNode(list *list, listNode *old_node, void *value, int after) {
	listNode *node;
	if ((node = (listNode *)zmalloc(sizeof(*node))) == NULL)
		return NULL;

	node->value = value;
	if (after) {
		node->prev = old_node;
		node->next = old_node->next;
		if (list->tail == old_node) {
			list->tail = node;
		}
	}
	else {
		node->next = old_node;
		node->prev = old_node->prev;
		if (list->head == old_node) {
			list->head = node;
		}
	}

	if (node->prev != NULL) {
		node->prev->next = node;
	}
	if (node->next != NULL) {
		node->next->prev = node;
	}

	list->len++;
	return list;
}


/*
* listDelNode 从链表 list 中删除给定节点 node
*
* 对节点私有值(private value of the node)的释放工作由调用者进行。该函数一定会成功.
*
* T = O(1)
*/
void listDelNode(list *list, listNode *node)
{
	if (node->prev)
		node->prev->next = node->next;
	else
		list->head = node->next;

	if (node->next)
		node->next->prev = node->prev;
	else
		list->tail = node->prev;

	if (list->free) list->free(node->value);
	zfree(node);
	list->len--;
}

/*
* 返回链表在给定索引上的值。
*
* 索引以 0 为起始，也可以是负数， -1 表示链表最后一个节点，诸如此类。
*
* 如果索引超出范围(out of range),返回 NULL 。
*
* T = O(N)
*/
listNode *listIndex(list *list, long index) {
	listNode *n;
	if (index < 0) {     // 如果索引为负数，从表尾开始查找
		index = (-index) - 1;
		n = list->tail;
		while (index-- && n) n = n->prev;
	}
	else {
		n = list->head;
		while (index-- && n) n = n->next;
	}

	return n;
}

/*
* 查找链表 list 中值和 key 匹配的节点。
*
* 对比操作由链表的 match 函数负责进行，
* 如果没有设置 match 函数，
* 那么直接通过对比值的指针来决定是否匹配。
*
* 如果匹配成功，那么第一个匹配的节点会被返回。
* 如果没有匹配任何节点，那么返回 NULL 。
*
* T = O(N)
*/
listNode *listSearchKey(list *list, void *key)
{
	listNode *node;
	listIter* iter = listGetIterator(list, AL_START_HEAD);
	while ((node = listNext(iter)) != NULL) {
		if (list->match) {
			if (list->match(node->value, key)) {
				listReleaseIterator(iter);
				return node;
			}
		}
		else {
			if (key == node->value) {
				listReleaseIterator(iter);
				return node;
			}
		}
	}
	listReleaseIterator(iter);
	return NULL;
}

/*
* 返回迭代器当前所指向的节点。
*
* 删除当前节点是允许的,但不能修改链表里的其他节点。
*
* 函数要么返回一个节点,要么返回 NULL,常见的用法是：
*
* iter = listGetIterator(list,<direction>);
* while ((node = listNext(iter)) != NULL) {
*     doSomethingWith(listNodeValue(node));
* }
*
* T = O(1)
*/
listNode *listNext(listIter *iter)
{
	listNode *current = iter->next;
	if (current != NULL) {
		if (iter->direction == AL_START_HEAD)
			iter->next = current->next;
		else
			iter->next = current->prev;
	}
	return current;
}

/*
* listGetIterator 为给定链表创建一个迭代器，
* 之后每次对这个迭代器调用 listNext 都返回被迭代到的链表节点,调用该函数不会失败
*
* direction 参数决定了迭代器的迭代方向：
*  AL_START_HEAD ：从表头向表尾迭代
*  AL_START_TAIL ：从表尾想表头迭代
*
* T = O(1)
*/
listIter *listGetIterator(list *list, int direction)
{
	listIter *iter;
	if ((iter = (listIter *)zmalloc(sizeof(*iter))) == NULL) return NULL;

	// 根据迭代方向，设置迭代器的起始节点
	if (direction == AL_START_HEAD)
		iter->next = list->head;
	else
		iter->next = list->tail;

	iter->direction = direction;

	return iter;
}

/*
* listReleaseIterator 释放迭代器
*
* T = O(1)
*/
void listReleaseIterator(listIter *iter) {
	zfree(iter);
}


int main(int argc, char* argv[])
{
	list* li = listCreate();
	listAddNodeHead(li, "item1");
	listAddNodeHead(li, "item2");

	listNode* node = listSearchKey(li, "item1");
	return 0;
}

