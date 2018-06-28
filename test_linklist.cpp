// test_ziplist.cpp : �������̨Ӧ�ó������ڵ㡣
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


typedef struct list { // ��c������,�ýṹ��ķ�ʽ��ģ�������һ�ֳ������ַ�
	listNode *head;
	listNode *tail;
	void *(*dup)(void *ptr);
	void(*free)(void *ptr);
	int(*match)(void *ptr, void *key);
	unsigned long len;
} list;

/* Functions implemented as macros */

// listLength ���ظ��������������Ľڵ�����
// T = O(1)
#define listLength(l) ((l)->len)

// listFirst ���ظ�������ı�ͷ�ڵ�
// T = O(1)
#define listFirst(l) ((l)->head)

// listLast ���ظ�������ı�β�ڵ�
// T = O(1)
#define listLast(l) ((l)->tail)

// listPrevNode ���ظ����ڵ��ǰ�ýڵ�
// T = O(1)
#define listPrevNode(n) ((n)->prev)

// listNextNode ���ظ����ڵ�ĺ��ýڵ�
// T = O(1)
#define listNextNode(n) ((n)->next)

// listNodeValue ���ظ����ڵ��ֵ
// T = O(1)
#define listNodeValue(n) ((n)->value)

// listSetDupMethod ������ l ��ֵ���ƺ�������Ϊ m
// T = O(1)
#define listSetDupMethod(l,m) ((l)->dup = (m))

// listSetFreeMethod ������ l ��ֵ�ͷź�������Ϊ m
// T = O(1)
#define listSetFreeMethod(l,m) ((l)->free = (m))

// listSetMatchMethod ������ĶԱȺ�������Ϊ m
// T = O(1)
#define listSetMatchMethod(l,m) ((l)->match = (m))

// listGetDupMethod ���ظ��������ֵ���ƺ���
// T = O(1) 
#define listGetDupMethod(l) ((l)->dup)

// listGetFree���ظ��������ֵ�ͷź���
// T = O(1)
#define listGetFree(l) ((l)->free)

// listGetMatchMethod ���ظ��������ֵ�ԱȺ���
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

/* Directions for iterators -- ���������е����ķ��� */
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
* listRelease �ͷ����������Լ����������нڵ�, ������������ܻ�ʧ��.
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
* listAddNodeHead ��һ�������и���ֵָ�� value ���½ڵ���ӵ�����ı�ͷ
*
* ���Ϊ�½ڵ�����ڴ������ô��ִ���κζ����������� NULL
*
* ���ִ�гɹ������ش��������ָ��
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
	else { // ��ӽڵ㵽�ǿ�����
		node->prev = NULL;
		node->next = list->head;
		list->head->prev = node;
		list->head = node;
	}
	list->len++;
	return list;
}


/*
* listAddNodeTail ��һ�������и���ֵָ�� value ���½ڵ���ӵ�����ı�β
*
* ���Ϊ�½ڵ�����ڴ������ô��ִ���κζ����������� NULL
*
* ���ִ�гɹ������ش��������ָ��
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
* listInsertNode ����һ������ֵ value ���½ڵ㣬���������뵽 old_node ��֮ǰ��֮��
*
* ��� after Ϊ 0 �����½ڵ���뵽 old_node ֮ǰ��
* ��� after Ϊ 1 �����½ڵ���뵽 old_node ֮��
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
* listDelNode ������ list ��ɾ�������ڵ� node
*
* �Խڵ�˽��ֵ(private value of the node)���ͷŹ����ɵ����߽��С��ú���һ����ɹ�.
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
* ���������ڸ��������ϵ�ֵ��
*
* ������ 0 Ϊ��ʼ��Ҳ�����Ǹ����� -1 ��ʾ�������һ���ڵ㣬������ࡣ
*
* �������������Χ(out of range),���� NULL ��
*
* T = O(N)
*/
listNode *listIndex(list *list, long index) {
	listNode *n;
	if (index < 0) {     // �������Ϊ�������ӱ�β��ʼ����
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
* �������� list ��ֵ�� key ƥ��Ľڵ㡣
*
* �ԱȲ���������� match ����������У�
* ���û������ match ������
* ��ôֱ��ͨ���Ա�ֵ��ָ���������Ƿ�ƥ�䡣
*
* ���ƥ��ɹ�����ô��һ��ƥ��Ľڵ�ᱻ���ء�
* ���û��ƥ���κνڵ㣬��ô���� NULL ��
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
* ���ص�������ǰ��ָ��Ľڵ㡣
*
* ɾ����ǰ�ڵ��������,�������޸�������������ڵ㡣
*
* ����Ҫô����һ���ڵ�,Ҫô���� NULL,�������÷��ǣ�
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
* listGetIterator Ϊ����������һ����������
* ֮��ÿ�ζ�������������� listNext �����ر�������������ڵ�,���øú�������ʧ��
*
* direction ���������˵������ĵ�������
*  AL_START_HEAD ���ӱ�ͷ���β����
*  AL_START_TAIL ���ӱ�β���ͷ����
*
* T = O(1)
*/
listIter *listGetIterator(list *list, int direction)
{
	listIter *iter;
	if ((iter = (listIter *)zmalloc(sizeof(*iter))) == NULL) return NULL;

	// ���ݵ����������õ���������ʼ�ڵ�
	if (direction == AL_START_HEAD)
		iter->next = list->head;
	else
		iter->next = list->tail;

	iter->direction = direction;

	return iter;
}

/*
* listReleaseIterator �ͷŵ�����
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

