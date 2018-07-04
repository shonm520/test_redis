
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_L 16 //������  

//new_node����һ��Node�ṹ��,ͬʱ���ɰ���n��Node *Ԫ�ص�����
#define new_node(n) ((Node*)malloc(sizeof(Node)+n*sizeof(Node*)))
//����key��value������ 
typedef int keyType;
typedef int valueType;

//ÿ���ڵ�����ݽṹ
typedef struct node
{
	//keyType key;// keyֵ
	valueType value;// valueֵ
	struct node * next[1];// ���ָ�����飬�������� ��ʵ�ֽṹ��ı䳤
} Node;
//����ṹ
typedef struct skip_list
{
	int level;// ������
	Node *head;//ָ��ͷ���
} skip_list;


Node *create_node(int level, valueType val);

skip_list *create_sl();
/*
����Ԫ�ص�ʱ��Ԫ����ռ�еĲ�����ȫ������㷨,�����������
*/
int randomLevel();

bool insert(skip_list *sl, valueType val);

bool erase(skip_list *sl, keyType key);

valueType *search(skip_list *sl, keyType key);
/*
����߲㿪ʼ����ӡ
sl ����ָ��
*/
void print_sl(skip_list *sl);

void sl_free(skip_list *sl);


// �����ڵ�
Node *create_node(int level, valueType val)
{
	Node *p = new_node(level);
	if (!p)
		return NULL;
	p->value = val;
	return p;
}

//������Ծ��
skip_list *create_sl()
{
	skip_list *sl = (skip_list*)malloc(sizeof(skip_list));//��������ṹ�ڴ�
	if (NULL == sl)
		return NULL;

	sl->level = 0;// ��������Ĳ�level����ʼ�Ĳ�Ϊ0�㣨�����0��ʼ��

	Node *h = create_node(MAX_L - 1, 0);//����ͷ���
	if (h == NULL)  {
		free(sl);
		return NULL;
	}
	sl->head = h;
	int i;
	// ��header��next�������
	for (i = 0; i < MAX_L; ++i)  {
		h->next[i] = NULL;
	}
	srand(time(0));
	return sl;
}
//����Ԫ�ص�ʱ��Ԫ����ռ�еĲ�����ȫ������㷨
int randomLevel()
{
	int level = 1;
	while (rand() % 2)
		level++;
	level = (MAX_L > level) ? level : MAX_L;
	return level;
}
/*
step1:���ҵ���ÿ�������λ��,����update����
step2:��Ҫ�������һ������
step3:�Ӹ߲����²���,����ͨ����Ĳ�����ȫ��ͬ��
*/
bool insert(skip_list *sl, valueType val)
{
	Node *update[MAX_L];
	Node *q = NULL, *p = sl->head;//q,p��ʼ��
	int i = sl->level - 1;
	/******************step1*******************/
	//����߲����²�����Ҫ�����λ��,������update
	//���ѽ���ڵ�ָ�뱣�浽update����
	for (; i >= 0; --i)  {
		while ((q = p->next[i]) && q->value < val)
			p = q;
		update[i] = p;
	}
	if (q && q->value == val)  {  //key�Ѿ����ڵ������
		q->value = val;
		return true;
	}
	/******************step2*******************/
	//����һ���������level
	int level = randomLevel();
	//��������ɵĲ���������Ĳ�����
	if (level > sl->level)  {
		//��update�����н�����ӵĲ�ָ��header;
		for (i = sl->level; i < level; ++i)  {
			update[i] = sl->head;
		}
		sl->level = level;
	}
	//printf("%d\n", sizeof(Node)+level*sizeof(Node*));
	/******************step3*******************/
	//�½�һ��������ڵ�,һ��һ�����
	q = create_node(level, val);
	if (!q)
		return false;

	//�����½ڵ��ָ��,����ͨ�������һ��
	for (i = level - 1; i >= 0; --i)  {
		q->next[i] = update[i]->next[i];
		update[i]->next[i] = q;
	}
	return true;
}
// ɾ���ڵ�
bool erase(skip_list *sl, valueType val)
{
	Node *update[MAX_L];
	Node *q = NULL, *p = sl->head;
	int i = sl->level - 1;
	for (; i >= 0; --i)  {
		while ((q = p->next[i]) && q->value < val)  {
			p = q;
		}
		update[i] = p;
	}
	//�ж��Ƿ�Ϊ��ɾ����key
	if (!q || (q&&q->value != val))
		return false;

	//���ɾ������ͨ����ɾ��һ��
	for (i = sl->level - 1; i >= 0; --i)  {
		if (update[i]->next[i] == q)  {  //ɾ���ڵ�
			update[i]->next[i] = q->next[i];
			//���ɾ��������߲�Ľڵ�,��level--
			if (sl->head->next[i] == NULL)
				sl->level--;
		}
	}
	free(q);
	q = NULL;
	return true;
}
// ����
valueType *search(skip_list *sl, keyType key)
{
	Node *q, *p = sl->head;
	q = NULL;
	int i = sl->level - 1;
	for (; i >= 0; --i)  {
		while ((q = p->next[i]) && q->value < key)  {
			p = q;
		}
		if (q && key == q->value)
			return &(q->value);
	}
	return NULL;
}

//����߲㿪ʼ����ӡ
void print_sl(skip_list *sl)
{
	Node *q;
	int i = sl->level - 1;
	for (; i >= 0; --i)  {
		q = sl->head->next[i];
		printf("level %d:\n", i + 1);
		while (q)  {
			printf("  val:%d\t", q->value);
			q = q->next[i];
		}
		printf("\n");
	}
}

// �ͷ���Ծ��
void sl_free(skip_list *sl)
{
	if (!sl)
		return;

	Node *q = sl->head;
	Node *next;
	while (q)  {
		next = q->next[0];
		free(q);
		q = next;
	}
	free(sl);
}



int main()
{
	int a[] = { 5, 8, 1, 6, 3, 2, 7, 4, 9 };
	skip_list *sl = create_sl();
	int i = 0;
	for (; i < 9; ++i)
	{
		insert(sl, a[i]);
	}
	erase(sl, 5);
	print_sl(sl);
	int *p = search(sl, 5);
	if (p)
		printf("value %d found!!!\n", *p);
	sl_free(sl);
	return 0;
}