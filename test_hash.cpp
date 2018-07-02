// test_ziplist.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <time.h>




#ifdef _WIN32
#include <winsock2.h>
#include <time.h>
#else
#include <sys/time.h>
#endif

unsigned long long GetCurrentTimeMsec()
{
#ifdef _WIN32
	struct timeval tv;
	time_t clock;
	struct tm tm;
	SYSTEMTIME wtm;

	GetLocalTime(&wtm);
	tm.tm_year = wtm.wYear - 1900;
	tm.tm_mon = wtm.wMonth - 1;
	tm.tm_mday = wtm.wDay;
	tm.tm_hour = wtm.wHour;
	tm.tm_min = wtm.wMinute;
	tm.tm_sec = wtm.wSecond;
	tm.tm_isdst = -1;
	clock = mktime(&tm);
	tv.tv_sec = clock;
	tv.tv_usec = wtm.wMilliseconds * 1000;
	return ((unsigned long long)tv.tv_sec * 1000 + (unsigned long long)tv.tv_usec / 1000);
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return ((unsigned long long)tv.tv_sec * 1000 + (unsigned long long)tv.tv_usec / 1000);
#endif
}

#define timeInMilliseconds GetCurrentTimeMsec


#define zmalloc malloc
#define zfree free
#define zcalloc calloc
#define random rand

// �����ɹ�
#define DICT_OK 0
// ����ʧ�ܣ������
#define DICT_ERR 1

typedef unsigned int       uint32_t;
typedef long long          int64_t;
typedef unsigned long long uint64_t;




typedef struct dictEntry {
	void *key;
	union {
		void *val;
		uint64_t u64;
		int64_t s64;
	} v;
	struct dictEntry *next;

} dictEntry;

//
// dictht ��ϣ��
//ÿ���ֵ䶼ʹ��������ϣ���Ӷ�ʵ�ֽ���ʽ rehash
// 
typedef struct dictht { // �����ֵ��ͷ��
	dictEntry **table;
	unsigned long size;
	unsigned long sizemask;
	unsigned long used;
} dictht;

//
// dictType ���ڲ����ֵ����ͺ���
//
typedef struct dictType {
	// �����ϣֵ�ĺ���
	unsigned int(*hashFunction)(const void *key);
	// ���Ƽ��ĺ���
	void *(*keyDup)(void *privdata, const void *key);
	// ����ֵ�ĺ���
	void *(*valDup)(void *privdata, const void *obj);
	// �Աȼ��ĺ���
	int(*keyCompare)(void *privdata, const void *key1, const void *key2);
	// ���ټ��ĺ���
	void(*keyDestructor)(void *privdata, void *key);
	// ����ֵ�ĺ���
	void(*valDestructor)(void *privdata, void *obj);
} dictType;

//
// dict �ֵ�
//
typedef struct dict {
	dictType *type; // type������Ҫ��¼��һϵ�еĺ���,����˵�ǹ涨��һϵ�еĽӿ�
	void *privdata; // privdata��������Ҫ���ݸ���Щ�����ض������Ŀ�ѡ����
	dictht ht[2]; // ������hash��
	int rehashidx; /* rehashing not in progress if rehashidx == -1 */
	int iterators; // Ŀǰ�������еİ�ȫ������������
} dict;


typedef struct dictIterator {
	dict *d;
	int table, index, safe;
	dictEntry *entry, *nextEntry;
	long long fingerprint; // unsafe iterator fingerprint for misuse detection
} dictIterator;


// dictEntry ��ϣ��ڵ�
//



// �ͷŸ����ֵ�ڵ��ֵ
#define dictFreeVal(d, entry) \
if ((d)->type->valDestructor) \
	(d)->type->valDestructor((d)->privdata, (entry)->v.val)

// ���ø����ֵ�ڵ��ֵ
#define dictSetVal(d, entry, _val_) do { \
if ((d)->type->valDup) \
	entry->v.val = (d)->type->valDup((d)->privdata, _val_); \
	else \
	entry->v.val = (_val_); \
	} while (0)

// ��һ���з���������Ϊ�ڵ��ֵ
#define dictSetSignedIntegerVal(entry, _val_) \
do { entry->v.s64 = _val_; } while (0)

// ��һ���޷���������Ϊ�ڵ��ֵ
#define dictSetUnsignedIntegerVal(entry, _val_) \
do { entry->v.u64 = _val_; } while (0)

// �ͷŸ����ֵ�ڵ�ļ�
#define dictFreeKey(d, entry) \
if ((d)->type->keyDestructor) \
	(d)->type->keyDestructor((d)->privdata, (entry)->key)

// ���ø����ֵ�ڵ�ļ�
#define dictSetKey(d, entry, _key_) do { \
if ((d)->type->keyDup) \
	entry->key = (d)->type->keyDup((d)->privdata, _key_); \
	else \
	entry->key = (_key_); \
	} while (0)

// �ȶ�������
#define dictCompareKeys(d, key1, key2) \
	(((d)->type->keyCompare) ? \
	(d)->type->keyCompare((d)->privdata, key1, key2) : \
	(key1) == (key2))

// ����������Ĺ�ϣֵ
#define dictHashKey(d, key) (d)->type->hashFunction(key)
// ���ػ�ȡ�����ڵ�ļ�
#define dictGetKey(he) ((he)->key)
// ���ػ�ȡ�����ڵ��ֵ
#define dictGetVal(he) ((he)->v.val)
// ���ػ�ȡ�����ڵ���з�������ֵ
#define dictGetSignedIntegerVal(he) ((he)->v.s64)
// ���ظ����ڵ���޷�������ֵ
#define dictGetUnsignedIntegerVal(he) ((he)->v.u64)
// ���ظ����ֵ�Ĵ�С
#define dictSlots(d) ((d)->ht[0].size+(d)->ht[1].size)
// �����ֵ�����нڵ�����
#define dictSize(d) ((d)->ht[0].used+(d)->ht[1].used)
// �鿴�ֵ��Ƿ����� rehash
#define dictIsRehashing(ht) ((ht)->rehashidx != -1)

/* API */
dict *dictCreate(dictType *type, void *privDataPtr);
int dictExpand(dict *d, unsigned long size);
int dictAdd(dict *d, void *key, void *val);
dictEntry *dictAddRaw(dict *d, void *key);
int dictReplace(dict *d, void *key, void *val);
dictEntry *dictReplaceRaw(dict *d, void *key);
int dictDelete(dict *d, const void *key);
int dictDeleteNoFree(dict *d, const void *key);
void dictRelease(dict *d);
dictEntry * dictFind(dict *d, const void *key);
void *dictFetchValue(dict *d, const void *key);
int dictResize(dict *d);
dictIterator *dictGetIterator(dict *d);
dictIterator *dictGetSafeIterator(dict *d);
dictEntry *dictNext(dictIterator *iter);
void dictReleaseIterator(dictIterator *iter);
dictEntry *dictGetRandomKey(dict *d);
int dictGetRandomKeys(dict *d, dictEntry **des, int count);
unsigned int dictGenHashFunction(const void *key, int len);
unsigned int dictGenCaseHashFunction(const unsigned char *buf, int len);
void dictEmpty(dict *d, void(callback)(void*));
void dictEnableResize(void);
void dictDisableResize(void);
int dictRehash(dict *d, int n);
int dictRehashMilliseconds(dict *d, int ms);
unsigned int dictGetHashFunctionSeed(void);

typedef void (dictScanFunction)(void *privdata, const dictEntry *de);

unsigned long dictScan(dict *d, unsigned long v, dictScanFunction *fn, void *privdata);

#define DICT_HT_INITIAL_SIZE     4

// ָʾ�ֵ��Ƿ����� rehash �ı�ʶ
static int dict_can_resize = 1;
// ǿ�� rehash �ı���
static unsigned int dict_force_resize_ratio = 5;

/* -------------------------- private prototypes ---------------------------- */

static int _dictExpandIfNeeded(dict *ht);
static unsigned long _dictNextPower(unsigned long size);
static int _dictKeyIndex(dict *ht, const void *key);
static int _dictInit(dict *ht, dictType *type, void *privDataPtr);

/* -------------------------- hash functions -------------------------------- */


/* Thomas Wang's 32 bit Mix Function */
unsigned int dictIntHashFunction(unsigned int key) {
	key += ~(key << 15);
	key ^= (key >> 10);
	key += (key << 3);
	key ^= (key >> 6);
	key += ~(key << 11);
	key ^= (key >> 16);
	return key;
}

/* Identity hash function for integer keys */
unsigned int dictIdentityHashFunction(unsigned int key) {
	return key;
}

static uint32_t dict_hash_function_seed = 5381;



/* This is our hash table structure. Every dictionary has two of this as we
* implement incremental rehashing, for the old to the new table. */




void dictSetHashFunctionSeed(uint32_t seed) {
	dict_hash_function_seed = seed;
}

/*
* ��ȡ����ֵ
*/
uint32_t dictGetHashFunctionSeed(void) {
	return dict_hash_function_seed;
}

/* MurmurHash2, by Austin Appleby
* Note - This code makes a few assumptions about how your machine behaves -
* 1. We can read a 4-byte value from any address without crashing
* 2. sizeof(int) == 4
*
* And it has a few limitations -
*
* 1. It will not work incrementally.
* 2. It will not produce the same results on little-endian and big-endian
*    machines.
*/
unsigned int dictGenHashFunction(const void *key, int len) {
	/* 'm' and 'r' are mixing constants generated offline.
	They're not really 'magic', they just happen to work well.  */
	uint32_t seed = dict_hash_function_seed;
	const uint32_t m = 0x5bd1e995;
	const int r = 24;

	/* Initialize the hash to a 'random' value */
	uint32_t h = seed ^ len;

	/* Mix 4 bytes at a time into the hash */
	const unsigned char *data = (const unsigned char *)key;

	while (len >= 4) {
		uint32_t k = *(uint32_t*)data;

		k *= m;
		k ^= k >> r;
		k *= m;

		h *= m;
		h ^= k;

		data += 4;
		len -= 4;
	}

	/* Handle the last few bytes of the input array  */
	switch (len) {
	case 3: h ^= data[2] << 16;
	case 2: h ^= data[1] << 8;
	case 1: h ^= data[0]; h *= m;
	};

	/* Do a few final mixes of the hash to ensure the last few
	* bytes are well-incorporated. */
	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;

	return (unsigned int)h;
}

/* And a case insensitive hash function (based on djb hash) */
unsigned int dictGenCaseHashFunction(const unsigned char *buf, int len) {
	unsigned int hash = (unsigned int)dict_hash_function_seed;

	while (len--)
		hash = ((hash << 5) + hash) + (tolower(*buf++)); /* hash * 33 + c */
	return hash;
}

/* ----------------------------- API implementation ------------------------- */

/*
* ���ã����ʼ����������ϣ��ĸ�������ֵ
*
* T = O(1)
*/
static void _dictReset(dictht *ht) {
	ht->table = NULL;
	ht->size = 0;
	ht->sizemask = 0;
	ht->used = 0;
}

/*
* ����һ���µ��ֵ�
*
* T = O(1)
*/
dict *dictCreate(dictType *type, void *privDataPtr) {
	dict *d = (dict*)zmalloc(sizeof(*d));
	_dictInit(d, type, privDataPtr);
	return d;
}

/*
* ��ʼ����ϣ��
*
* T = O(1)
*/
int _dictInit(dict *d, dictType *type,	void *privDataPtr) {
	// ��ʼ��������ϣ��ĸ�������ֵ
	// ����ʱ���������ڴ����ϣ������
	_dictReset(&d->ht[0]);
	_dictReset(&d->ht[1]);

	d->type = type;
	d->privdata = privDataPtr;
	d->rehashidx = -1;
	d->iterators = 0;

	return DICT_OK; 
}

/*
* ��С�����ֵ�
* ���������ýڵ������ֵ��С֮��ı��ʽӽ� 1:1
*
* ���� DICT_ERR ��ʾ�ֵ��Ѿ��� rehash ������ dict_can_resize Ϊ�١�
*
* �ɹ����������С�� ht[1] �����Կ�ʼ resize ʱ������ DICT_OK��
*
* T = O(N)
*/
int dictResize(dict *d) {
	int minimal;

	// �����ڹر� rehash �������� rehash ��ʱ�����
	if (!dict_can_resize || dictIsRehashing(d)) return DICT_ERR;

	// �����ñ��ʽӽ� 1��1 ����Ҫ�����ٽڵ�����
	minimal = d->ht[0].used;
	if (minimal < DICT_HT_INITIAL_SIZE)
		minimal = DICT_HT_INITIAL_SIZE;

	// �����ֵ�Ĵ�С
	// T = O(N)
	return dictExpand(d, minimal);
}

/*
* ����һ���µĹ�ϣ���������ֵ�������ѡ����������һ�����������У�
*
* 1) ����ֵ�� 0 �Ź�ϣ��Ϊ�գ���ô���¹�ϣ������Ϊ 0 �Ź�ϣ��
* 2) ����ֵ�� 0 �Ź�ϣ��ǿգ���ô���¹�ϣ������Ϊ 1 �Ź�ϣ��
*    �����ֵ�� rehash ��ʶ��ʹ�ó�����Կ�ʼ���ֵ���� rehash
*
* size ���������󣬻��� rehash �Ѿ��ڽ���ʱ������ DICT_ERR ��
*
* �ɹ����� 0 �Ź�ϣ������ 1 �Ź�ϣ��ʱ������ DICT_OK ��
*
* T = O(N)
*/
int dictExpand(dict *d, unsigned long size) {
	// �¹�ϣ��
	dictht n;

	// ���� size �����������ϣ��Ĵ�С
	// T = O(1)
	unsigned long realsize = _dictNextPower(size);

	// �������ֵ����� rehash ʱ����
	// size ��ֵҲ����С�� 0 �Ź�ϣ��ĵ�ǰ��ʹ�ýڵ�
	if (dictIsRehashing(d) || d->ht[0].used > size)
		return DICT_ERR;

	// Ϊ��ϣ�����ռ䣬��������ָ��ָ�� NULL
	n.size = realsize;
	n.sizemask = realsize - 1;
	// T = O(N)
	n.table = (dictEntry **)zcalloc(1, realsize * sizeof(dictEntry*)); // dicEntry��һ������
	n.used = 0;

	// ��� 0 �Ź�ϣ��Ϊ�գ���ô����һ�γ�ʼ����
	// �����¹�ϣ���� 0 �Ź�ϣ���ָ�룬Ȼ���ֵ�Ϳ��Կ�ʼ�����ֵ���ˡ�
	if (d->ht[0].table == NULL) {
		d->ht[0] = n; // ht[0]��dictht���͵�,���ǲ���int���͵İ�.�ð�,n��dictht���͵�,����,���Ǻ����ڵ�һ���ֲ�����
		// û��,����ʵ�����ڴ�Ŀ���
		return DICT_OK;
	}

	// Prepare a second hash table for incremental rehashing
	// ��� 0 �Ź�ϣ��ǿգ���ô����һ�� rehash ��
	// �����¹�ϣ������Ϊ 1 �Ź�ϣ��
	// �����ֵ�� rehash ��ʶ�򿪣��ó�����Կ�ʼ���ֵ���� rehash
	d->ht[1] = n;
	// rehashidx���õ÷ǳ�Ư��,û��rehashʱ,rehashidxΪ-1, һ��
	// ��ʼrehashʱ,rehashidx�趨Ϊ0,��ʾ��ht[0]��ĵ�0��Ԫ�ؿ�ʼrehash
	// Ȼ��rehashidx������,������Ϊָʾ��,���Խ�ht[0]���е�����Ԫ�ض�rehash��
	d->rehashidx = 0;
	return DICT_OK;

	/* ˳��һ�ᣬ����Ĵ�������ع���������ʽ��

	if (d->ht[0].table == NULL) {
	// ��ʼ��
	d->ht[0] = n;
	}
	else {
	// rehash
	d->ht[1] = n;
	d->rehashidx = 0;
	}

	return DICT_OK;

	*/
}

/*
* ִ�� N ������ʽ rehash ��
*
* ���� 1 ��ʾ���м���Ҫ�� 0 �Ź�ϣ���ƶ��� 1 �Ź�ϣ��
* ���� 0 ���ʾ���м����Ѿ�Ǩ����ϡ�
*
* ע�⣬ÿ�� rehash ������һ����ϣ��������Ͱ����Ϊ��λ�ģ�
* һ��Ͱ����ܻ��ж���ڵ㣬
* �� rehash ��Ͱ������нڵ㶼�ᱻ�ƶ����¹�ϣ��
*
* T = O(N)
*/
int dictRehash(dict *d, int n) {
	if (!dictIsRehashing(d)) return 0;
	while (n--) { 
		dictEntry *de, *nextde;
		if (d->ht[0].used == 0) {   // ��� 0 �Ź�ϣ��Ϊ�գ���ô��ʾ rehash ִ�����
			zfree(d->ht[0].table);
			d->ht[0] = d->ht[1];
			_dictReset(&d->ht[1]);
			d->rehashidx = -1;
			return 0;
		}

		// Note that rehashidx can't overflow as we are sure there are more
		// elements because ht[0].used != 0
		// ȷ�� rehashidx û��Խ��
		assert(d->ht[0].size > (unsigned)d->rehashidx);

		while (d->ht[0].table[d->rehashidx] == NULL) d->rehashidx++;    // �Թ�������Ϊ�յ��������ҵ���һ���ǿ�����

		de = d->ht[0].table[d->rehashidx];
		while (de) {
			unsigned int h;
			nextde = de->next;
			// �����¹�ϣ��Ĺ�ϣֵ���Լ��ڵ���������λ��
			h = dictHashKey(d, de->key) & d->ht[1].sizemask;
			// ����ڵ㵽�¹�ϣ��,�����ǲ��뵽��ͷ
			de->next = d->ht[1].table[h];
			d->ht[1].table[h] = de;

			d->ht[0].used--;
			d->ht[1].used++;
			de = nextde;
		}
		// ����Ǩ����Ĺ�ϣ��������ָ����Ϊ��
		d->ht[0].table[d->rehashidx] = NULL;
		d->rehashidx++;
	}
	return 1;
}



/*
* �ڸ����������ڣ��� 100 ��Ϊ��λ, ���ֵ���� rehash.Ҳ����˵ÿ�ζ�100��dictEntry����hash.
*
* T = O(N)
*/
int dictRehashMilliseconds(dict *d, int ms) {
	// ��¼��ʼʱ��
	long long start = timeInMilliseconds(); // ��ʼ��ʱ��
	int rehashes = 0;

	while (dictRehash(d, 100)) {
		rehashes += 100;
		// ���ʱ���ѹ�������
		if (timeInMilliseconds() - start > ms) break;
	}

	return rehashes;
}

/*
* ���ֵ䲻���ڰ�ȫ������������£����ֵ���е��� rehash ��
*
* �ֵ��а�ȫ������������²��ܽ��� rehash ��
* ��Ϊ���ֲ�ͬ�ĵ������޸Ĳ������ܻ�Ū���ֵ䡣
*
* ������������ͨ�õĲ��ҡ����²������ã�
* ���������ֵ��ڱ�ʹ�õ�ͬʱ���� rehash ��
*
* T = O(1)
*/
static void _dictRehashStep(dict *d) {
	if (d->iterators == 0) dictRehash(d, 1);
}

/*
* ���Խ�������ֵ����ӵ��ֵ���
*
* ֻ�и����� key ���������ֵ�ʱ����Ӳ����Ż�ɹ�
*
* ��ӳɹ����� DICT_OK , ʧ�ܷ��� DICT_ERR
*
* � T = O(N),ƽ̯ O(1)
*/
int dictAdd(dict *d, void *key, void *val) // ���һ����ֵ�Խ��뵽dict��
{
	// ������Ӽ����ֵ䣬�����ذ�������������¹�ϣ�ڵ�
	// T = O(N)
	dictEntry *entry = dictAddRaw(d, key);

	// ���Ѵ��ڣ����ʧ��
	if (!entry) return DICT_ERR;

	// �������ڣ����ýڵ��ֵ
	// T = O(1)
	dictSetVal(d, entry, val);

	// ��ӳɹ�
	return DICT_OK;
}

/*
* ���Խ������뵽�ֵ���
*
* ������Ѿ����ֵ���ڣ���ô���� NULL
*
* ����������ڣ���ô���򴴽��µĹ�ϣ�ڵ㣬
* ���ڵ�ͼ������������뵽�ֵ䣬Ȼ�󷵻ؽڵ㱾��
*
* T = O(N)
*/
dictEntry *dictAddRaw(dict *d, void *key)
{
	int index;
	dictEntry *entry;
	dictht *ht;

	// �����������Ļ������е��� rehash
	// T = O(1)
	// �����Ҫrehashing,��ô���ǽ���rehash,ע��,�����ǵ���rehash
	if (dictIsRehashing(d)) _dictRehashStep(d);

	// ������ڹ�ϣ���е�����ֵ
	// ���ֵΪ -1 ����ô��ʾ���Ѿ�����
	// T = O(N)
	if ((index = _dictKeyIndex(d, key)) == -1)
		return NULL;

	// T = O(1)
	// ����ֵ����� rehash ����ô���¼���ӵ� 1 �Ź�ϣ��
	// ���򣬽��¼���ӵ� 0 �Ź�ϣ��
	ht = dictIsRehashing(d) ? &d->ht[1] : &d->ht[0];
	entry = (dictEntry*)zmalloc(sizeof(*entry));
	entry->next = ht->table[index];
	ht->table[index] = entry;
	ht->used++;

	dictSetKey(d, entry, key);

	return entry;
}

/*
* �������ļ�ֵ����ӵ��ֵ��У�������Ѿ����ڣ���ôɾ�����еļ�ֵ�ԡ�
*
* �����ֵ��Ϊȫ����ӣ���ô���� 1 ��
* �����ֵ����ͨ����ԭ�еļ�ֵ�Ը��µ����ģ���ô���� 0 ��
*
* T = O(N)
*/
int dictReplace(dict *d, void *key, void *val)
{
	dictEntry *entry, auxentry;

	// ����ֱ�ӽ���ֵ����ӵ��ֵ�
	// ����� key �����ڵĻ�����ӻ�ɹ�
	// T = O(N)
	if (dictAdd(d, key, val) == DICT_OK)
		return 1;

	// ���е����˵���� key �Ѿ����ڣ���ô�ҳ�������� key �Ľڵ�
	// T = O(1)
	entry = dictFind(d, key);
	/* Set the new value and free the old one. Note that it is important
	* to do that in this order, as the value may just be exactly the same
	* as the previous one. In this context, think to reference counting,
	* you want to increment (set), and then decrement (free), and not the
	* reverse. */
	// �ȱ���ԭ�е�ֵ��ָ��
	auxentry = *entry;
	// Ȼ�������µ�ֵ
	// T = O(1)
	dictSetVal(d, entry, val);
	// Ȼ���ͷž�ֵ
	// T = O(1)
	dictFreeVal(d, &auxentry);

	return 0;
}

/* dictReplaceRaw() is simply a version of dictAddRaw() that always
* returns the hash entry of the specified key, even if the key already
* exists and can't be added (in that case the entry of the already
* existing key is returned.)
*
* See dictAddRaw() for more information. */
/*
* dictAddRaw() ���ݸ��� key �ͷŴ��ڣ�ִ�����¶�����
*
* 1) key �Ѿ����ڣ����ذ����� key ���ֵ�ڵ�
* 2) key �����ڣ���ô�� key ��ӵ��ֵ�
*
* ���۷������ϵ���һ�������
* dictAddRaw() �����Ƿ��ذ������� key ���ֵ�ڵ㡣
*
* T = O(N)
*/
dictEntry *dictReplaceRaw(dict *d, void *key) {

	// ʹ�� key ���ֵ��в��ҽڵ�
	// T = O(1)
	dictEntry *entry = dictFind(d, key);

	// ����ڵ��ҵ���ֱ�ӷ��ؽڵ㣬������Ӳ�����һ���½ڵ�
	// T = O(N)
	return entry ? entry : dictAddRaw(d, key);
}

/*
* ���Ҳ�ɾ�������������Ľڵ�
*
* ���� nofree �����Ƿ���ü���ֵ���ͷź���
* 0 ��ʾ���ã�1 ��ʾ������
*
* �ҵ����ɹ�ɾ������ DICT_OK ��û�ҵ��򷵻� DICT_ERR
*
* T = O(1)
*/
static int dictGenericDelete(dict *d, const void *key, int nofree) {
	unsigned int h, idx;
	dictEntry *he, *prevHe;
	int table;

	// �ֵ䣨�Ĺ�ϣ��Ϊ��
	if (d->ht[0].size == 0) return DICT_ERR; // d->ht[0].table is NULL

	// ���е��� rehash, T = O(1)
	if (dictIsRehashing(d)) _dictRehashStep(d); // �����������ʱ��ʱ�ؽ���rehash

	// �����ϣֵ
	h = dictHashKey(d, key);

	// ������ϣ��
	// T = O(1)
	for (table = 0; table <= 1; table++) { // ���ڿ�������rehash, ����Ҫ����������Ѱ��

		// ��������ֵ 
		idx = h & d->ht[table].sizemask;
		// ָ��������ϵ�����
		he = d->ht[table].table[idx];
		prevHe = NULL;
		// ���������ϵ����нڵ�
		// T = O(1)
		while (he) {

			if (dictCompareKeys(d, key, he->key)) {
				// ����Ŀ��ڵ�

				// ��������ɾ��
				if (prevHe)
					prevHe->next = he->next;
				else
					d->ht[table].table[idx] = he->next;

				// �ͷŵ��ü���ֵ���ͷź�����
				if (!nofree) {
					dictFreeKey(d, he);
					dictFreeVal(d, he);
				}

				// �ͷŽڵ㱾��
				zfree(he);

				// ������ʹ�ýڵ�����
				d->ht[table].used--;

				// �������ҵ��ź�
				return DICT_OK;
			}

			prevHe = he;
			he = he->next;
		}

		// ���ִ�е����˵���� 0 �Ź�ϣ�����Ҳ���������
		// ��ô�����ֵ��Ƿ����ڽ��� rehash ������Ҫ��Ҫ���� 1 �Ź�ϣ��
		if (!dictIsRehashing(d)) break;
	}

	// û�ҵ�
	return DICT_ERR;
}

/*
* ���ֵ���ɾ�������������Ľڵ�
*
* ���ҵ��ü�ֵ���ͷź�����ɾ����ֵ
*
* �ҵ����ɹ�ɾ������ DICT_OK ��û�ҵ��򷵻� DICT_ERR
* T = O(1)
*/
int dictDelete(dict *ht, const void *key) {
	return dictGenericDelete(ht, key, 0);
}

/*
* ���ֵ���ɾ�������������Ľڵ�
*
* �������ü�ֵ���ͷź�����ɾ����ֵ
*
* �ҵ����ɹ�ɾ������ DICT_OK ��û�ҵ��򷵻� DICT_ERR
* T = O(1)
*/
int dictDeleteNoFree(dict *ht, const void *key) {
	return dictGenericDelete(ht, key, 1); // ���û�б����µ�ַ�Ļ�,��������ڴ�й¶
}

/*
* ɾ����ϣ���ϵ����нڵ㣬�����ù�ϣ��ĸ�������
*
* T = O(N)
*/
int _dictClear(dict *d, dictht *ht, void(callback)(void *)) {
	unsigned long i;

	// ����������ϣ��
	// T = O(N)
	for (i = 0; i < ht->size && ht->used > 0; i++) {
		dictEntry *he, *nextHe;

		if (callback && (i & 65535) == 0) callback(d->privdata);

		// ����������
		if ((he = ht->table[i]) == NULL) continue;

		// ������������
		// T = O(1)
		while (he) {
			nextHe = he->next;
			// ɾ����
			dictFreeKey(d, he);
			// ɾ��ֵ
			dictFreeVal(d, he);
			// �ͷŽڵ�
			zfree(he);

			// ������ʹ�ýڵ����
			ht->used--;

			// �����¸��ڵ�
			he = nextHe;
		}
	}

	// �ͷŹ�ϣ��ṹ
	zfree(ht->table);

	// ���ù�ϣ������
	_dictReset(ht);

	return DICT_OK;
}

/*
* ɾ�����ͷ������ֵ�
*
* T = O(N)
*/
void dictRelease(dict *d)
{
	// ɾ�������������ϣ��
	_dictClear(d, &d->ht[0], NULL);
	_dictClear(d, &d->ht[1], NULL);
	// �ͷŽڵ�ṹ
	zfree(d);
}

/*
* �����ֵ��а����� key �Ľڵ�
*
* �ҵ����ؽڵ㣬�Ҳ������� NULL
*
* T = O(1)
*/
dictEntry *dictFind(dict *d, const void *key)
{
	dictEntry *he;
	unsigned int h, idx, table;

	if (d->ht[0].size == 0) return NULL;
	// �����������Ļ������е��� rehash
	if (dictIsRehashing(d)) _dictRehashStep(d);

	h = dictHashKey(d, key);
	for (table = 0; table <= 1; table++) {
		idx = h & d->ht[table].sizemask;
		he = d->ht[table].table[idx];
		while (he) {
			if (dictCompareKeys(d, key, he->key))
				return he;
			he = he->next;
		}

		// ������������ 0 �Ź�ϣ����Ȼû�ҵ�ָ���ļ��Ľڵ�
		// ��ô��������ֵ��Ƿ��ڽ��� rehash ��
		// Ȼ��ž�����ֱ�ӷ��� NULL �����Ǽ������� 1 �Ź�ϣ��
		if (!dictIsRehashing(d)) return NULL;
	}

	return NULL;
}

/*
* ��ȡ�����������Ľڵ��ֵ
*
* ����ڵ㲻Ϊ�գ����ؽڵ��ֵ
* ���򷵻� NULL
*
* T = O(1)
*/
void *dictFetchValue(dict *d, const void *key) {
	dictEntry *he;

	// T = O(1)
	he = dictFind(d, key);

	return he ? dictGetVal(he) : NULL;
}

/* A fingerprint is a 64 bit number that represents the state of the dictionary
* at a given time, it's just a few dict properties xored together.
* When an unsafe iterator is initialized, we get the dict fingerprint, and check
* the fingerprint again when the iterator is released.
* If the two fingerprints are different it means that the user of the iterator
* performed forbidden operations against the dictionary while iterating. */
long long dictFingerprint(dict *d) { // fingerprint��ָ�Ƶ���˼.һ��dict�����˸ı�,ָ��Ҳ�����˸ı�
	long long integers[6], hash = 0;
	int j;

	integers[0] = (long)d->ht[0].table;
	integers[1] = d->ht[0].size;
	integers[2] = d->ht[0].used;
	integers[3] = (long)d->ht[1].table;
	integers[4] = d->ht[1].size;
	integers[5] = d->ht[1].used;

	/* We hash N integers by summing every successive integer with the integer
	* hashing of the previous sum. Basically:
	*
	* Result = hash(hash(hash(int1)+int2)+int3) ...
	*
	* This way the same set of integers in a different order will (likely) hash
	* to a different number. */
	for (j = 0; j < 6; j++) {
		hash += integers[j];
		/* For the hashing step we use Tomas Wang's 64 bit integer hash. */
		hash = (~hash) + (hash << 21); // hash = (hash << 21) - hash - 1;
		hash = hash ^ (hash >> 24);
		hash = (hash + (hash << 3)) + (hash << 8); // hash * 265
		hash = hash ^ (hash >> 14);
		hash = (hash + (hash << 2)) + (hash << 4); // hash * 21
		hash = hash ^ (hash >> 28);
		hash = hash + (hash << 31);
	}
	return hash;
}

/*
* ���������ظ����ֵ�Ĳ���ȫ������
*
* T = O(1)
*/
dictIterator *dictGetIterator(dict *d) {
	dictIterator *iter = (dictIterator*)zmalloc(sizeof(*iter));

	iter->d = d;
	iter->table = 0;
	iter->index = -1;
	iter->safe = 0;
	iter->entry = NULL;
	iter->nextEntry = NULL;

	return iter;
}

/*
* ���������ظ����ڵ�İ�ȫ������
*
* T = O(1)
*/
dictIterator *dictGetSafeIterator(dict *d) { // ʲô������ȫ�ĵ�����?
	dictIterator *i = dictGetIterator(d);

	// ���ð�ȫ��������ʶ
	i->safe = 1; // ��ȫֻ��һ����ʶ������.

	return i;
}

/*
* ���ص�����ָ��ĵ�ǰ�ڵ�
*
* �ֵ�������ʱ������ NULL
*
* T = O(1)
*/
dictEntry *dictNext(dictIterator *iter) // Ҫ��֤��ȫ�Ļ�,ֻ��ʹ���ض��ĺ��������ݽ��в���.
{
	while (1) {
		// �������ѭ�������ֿ��ܣ�
		// 1) ���ǵ�������һ������
		// 2) ��ǰ���������еĽڵ��Ѿ������꣨NULL Ϊ����ı�β��
		if (iter->entry == NULL) {

			// ָ�򱻵����Ĺ�ϣ��
			dictht *ht = &iter->d->ht[iter->table]; // tableֻ��һ��int���͵�ָʾ������

			// ���ε���ʱִ��
			if (iter->index == -1 && iter->table == 0) {
				// ����ǰ�ȫ����������ô���°�ȫ������������
				if (iter->safe)
					iter->d->iterators++;
				// ����ǲ���ȫ����������ô����ָ��
				else
					iter->fingerprint = dictFingerprint(iter->d);
			}
			// ��������
			iter->index++;

			// ����������ĵ�ǰ�������ڵ�ǰ�������Ĺ�ϣ��Ĵ�С
			// ��ô˵�������ϣ���Ѿ��������
			if (iter->index >= (signed)ht->size) {
				// ������� rehash �Ļ�����ô˵�� 1 �Ź�ϣ��Ҳ����ʹ����
				// ��ô������ 1 �Ź�ϣ����е���
				if (dictIsRehashing(iter->d) && iter->table == 0) {
					iter->table++;
					iter->index = 0;
					ht = &iter->d->ht[1];
					// ���û�� rehash ����ô˵�������Ѿ����
				}
				else {
					break;
				}
			}

			// ������е����˵�������ϣ��δ������
			// ���½ڵ�ָ�룬ָ���¸���������ı�ͷ�ڵ�
			iter->entry = ht->table[iter->index];
		}
		else {
			// ִ�е����˵���������ڵ���ĳ������
			// ���ڵ�ָ��ָ��������¸��ڵ�
			iter->entry = iter->nextEntry;
		}

		// �����ǰ�ڵ㲻Ϊ�գ���ôҲ��¼�¸ýڵ���¸��ڵ�
		// ��Ϊ��ȫ�������п��ܻὫ���������صĵ�ǰ�ڵ�ɾ��
		if (iter->entry) {
			iter->nextEntry = iter->entry->next;
			return iter->entry;
		}
	}

	// �������
	return NULL;
}

/*
* �ͷŸ����ֵ������
*
* T = O(1)
*/
void dictReleaseIterator(dictIterator *iter)
{

	if (!(iter->index == -1 && iter->table == 0)) {
		// �ͷŰ�ȫ������ʱ����ȫ��������������һ
		if (iter->safe)
			iter->d->iterators--;
		// �ͷŲ���ȫ������ʱ����ָ֤���Ƿ��б仯
		else
			assert(iter->fingerprint == dictFingerprint(iter->d));
	}
	zfree(iter);
}

/*
* ��������ֵ�������һ���ڵ㡣
*
* ������ʵ��������㷨��
*
* ����ֵ�Ϊ�գ����� NULL ��
*
* T = O(N)
*/
dictEntry *dictGetRandomKey(dict *d)
{
	dictEntry *he, *orighe;
	unsigned int h;
	int listlen, listele;

	// �ֵ�Ϊ��
	if (dictSize(d) == 0) return NULL;

	// ���е��� rehash
	if (dictIsRehashing(d)) _dictRehashStep(d);

	// ������� rehash ����ô�� 1 �Ź�ϣ��Ҳ��Ϊ������ҵ�Ŀ��
	if (dictIsRehashing(d)) {
		// T = O(N)
		do {
			h = random() % (d->ht[0].size + d->ht[1].size);
			he = (h >= d->ht[0].size) ? d->ht[1].table[h - d->ht[0].size] :
				d->ht[0].table[h];
		} while (he == NULL);
		// ����ֻ�� 0 �Ź�ϣ���в��ҽڵ�
	}
	else {
		// T = O(N)
		do {
			h = random() & d->ht[0].sizemask;
			he = d->ht[0].table[h];
		} while (he == NULL);
	}

	// Ŀǰ he �Ѿ�ָ��һ���ǿյĽڵ�����
	// ���򽫴���������������һ���ڵ�
	listlen = 0;
	orighe = he;
	// ����ڵ�����, T = O(1)
	while (he) {
		he = he->next;
		listlen++;
	}
	// ȡģ���ó�����ڵ������
	listele = random() % listlen;
	he = orighe;
	// ���������ҽڵ�
	// T = O(1)
	while (listele--) he = he->next;

	// ��������ڵ�
	return he;
}

/* This is a version of dictGetRandomKey() that is modified in order to
* return multiple entries by jumping at a random place of the hash table
* and scanning linearly for entries.
*
* Returned pointers to hash table entries are stored into 'des' that
* points to an array of dictEntry pointers. The array must have room for
* at least 'count' elements, that is the argument we pass to the function
* to tell how many random elements we need.
*
* The function returns the number of items stored into 'des', that may
* be less than 'count' if the hash table has less than 'count' elements
* inside.
*
* Note that this function is not suitable when you need a good distribution
* of the returned items, but only when you need to "sample" a given number
* of continuous elements to run some kind of algorithm or to produce
* statistics. However the function is much faster than dictGetRandomKey()
* at producing N elements, and the elements are guaranteed to be non
* repeating. */
int dictGetRandomKeys(dict *d, dictEntry **des, int count) {
	int j; /* internal hash table id, 0 or 1. */
	int stored = 0;

	if (dictSize(d) < count) count = dictSize(d);
	while (stored < count) {
		for (j = 0; j < 2; j++) {
			/* Pick a random point inside the hash table 0 or 1. */
			unsigned int i = random() & d->ht[j].sizemask;
			int size = d->ht[j].size;

			/* Make sure to visit every bucket by iterating 'size' times. */
			while (size--) {
				dictEntry *he = d->ht[j].table[i];
				while (he) {
					/* Collect all the elements of the buckets found non
					* empty while iterating. */
					*des = he;
					des++;
					he = he->next;
					stored++;
					if (stored == count) return stored;
				}
				i = (i + 1) & d->ht[j].sizemask;
			}
			/* If there is only one table and we iterated it all, we should
			* already have 'count' elements. Assert this condition. */
			assert(dictIsRehashing(d) != 0);
		}
	}
	return stored; /* Never reached. */
}

/* Function to reverse bits. Algorithm from:
* http://graphics.stanford.edu/~seander/bithacks.html#ReverseParallel */
static unsigned long rev(unsigned long v) {
	unsigned long s = 8 * sizeof(v); // bit size; must be power of 2
	unsigned long mask = ~0;
	while ((s >>= 1) > 0) {
		mask ^= (mask << s);
		v = ((v >> s) & mask) | ((v << s) & ~mask);
	}
	return v;
}


/* dictScan() is used to iterate over the elements of a dictionary.
*
* dictScan() �������ڵ��������ֵ��е�Ԫ�ء�
*
* Iterating works in the following way:
*
* ���������·�ʽִ�У�
*
* 1) Initially you call the function using a cursor (v) value of 0.
*    һ��ʼ����ʹ�� 0 ��Ϊ�α������ú�����
* 2) The function performs one step of the iteration, and returns the
*    new cursor value that you must use in the next call.
*    ����ִ��һ������������
*    ������һ���´ε���ʱʹ�õ����αꡣ
* 3) When the returned cursor is 0, the iteration is complete.
*    ���������ص��α�Ϊ 0 ʱ��������ɡ�
*
* The function guarantees that all the elements that are present in the
* dictionary from the start to the end of the iteration are returned.
* However it is possible that some element is returned multiple time.
*
* ������֤���ڵ����ӿ�ʼ�������ڼ䣬һֱ�������ֵ��Ԫ�ؿ϶��ᱻ��������
* ��һ��Ԫ�ؿ��ܻᱻ���ض�Ρ�
*
* For every element returned, the callback 'fn' passed as argument is
* called, with 'privdata' as first argument and the dictionar entry
* 'de' as second argument.
*
* ÿ��һ��Ԫ�ر�����ʱ���ص����� fn �ͻᱻִ�У�
* fn �����ĵ�һ�������� privdata �����ڶ������������ֵ�ڵ� de ��
*
* HOW IT WORKS.
* ����ԭ��
*
* The algorithm used in the iteration was designed by Pieter Noordhuis.
* The main idea is to increment a cursor starting from the higher order
* bits, that is, instead of incrementing the cursor normally, the bits
* of the cursor are reversed, then the cursor is incremented, and finally
* the bits are reversed again.
*
* ������ʹ�õ��㷨���� Pieter Noordhuis ��Ƶģ�
* �㷨����Ҫ˼·���ڶ����Ƹ�λ�϶��α���мӷ�����
* Ҳ����˵�����ǰ������İ취�����α���мӷ����㣬
* �������Ƚ��α�Ķ�����λ��ת��reverse��������
* Ȼ��Է�ת���ֵ���мӷ����㣬
* ����ٴζԼӷ�����֮��Ľ�����з�ת��
*
* This strategy is needed because the hash table may be resized from one
* call to the other call of the same iteration.
*
* ��һ�����Ǳ�Ҫ�ģ���Ϊ��һ�������ĵ��������У�
* ��ϣ��Ĵ�С�п��������ε���֮�䷢���ı䡣
*
* dict.c hash tables are always power of two in size, and they
* use chaining, so the position of an element in a given table is given
* always by computing the bitwise AND between Hash(key) and SIZE-1
* (where SIZE-1 is always the mask that is equivalent to taking the rest
*  of the division between the Hash of the key and SIZE).
*
* ��ϣ��Ĵ�С���� 2 ��ĳ���η������ҹ�ϣ��ʹ�������������ͻ��
* ���һ������Ԫ����һ���������λ���ܿ���ͨ�� Hash(key) & SIZE-1
* ��ʽ������ó���
* ���� SIZE-1 �ǹ�ϣ����������ֵ��
* ����������ֵ���ǹ�ϣ��� mask �����룩��
*
* For example if the current hash table size is 16, the mask is
* (in binary) 1111. The position of a key in the hash table will be always
* the last four bits of the hash output, and so forth.
*
* �ٸ����ӣ������ǰ��ϣ��Ĵ�СΪ 16 ��
* ��ô����������Ƕ�����ֵ 1111 ��
* �����ϣ�������λ�ö�����ʹ�ù�ϣֵ������ĸ�������λ����¼��
*
* WHAT HAPPENS IF THE TABLE CHANGES IN SIZE?
* �����ϣ��Ĵ�С�ı�����ô�죿
*
* If the hash table grows, elements can go anyway in one multiple of
* the old bucket: for example let's say that we already iterated with
* a 4 bit cursor 1100, since the mask is 1111 (hash table size = 16).
*
* ���Թ�ϣ�������չʱ��Ԫ�ؿ��ܻ��һ�����ƶ�����һ���ۣ�
* �ٸ����ӣ��������Ǹպõ����� 4 λ�α� 1100 ��
* ����ϣ��� mask Ϊ 1111 ����ϣ��Ĵ�СΪ 16 ����
*
* If the hash table will be resized to 64 elements, and the new mask will
* be 111111, the new buckets that you obtain substituting in ??1100
* either 0 or 1, can be targeted only by keys that we already visited
* when scanning the bucket 1100 in the smaller hash table.
*
* �����ʱ��ϣ����С��Ϊ 64 ����ô��ϣ��� mask ����Ϊ 111111 ��
*
* By iterating the higher bits first, because of the inverted counter, the
* cursor does not need to restart if the table size gets bigger, and will
* just continue iterating with cursors that don't have '1100' at the end,
* nor any other combination of final 4 bits already explored.
*
* Similarly when the table size shrinks over time, for example going from
* 16 to 8, If a combination of the lower three bits (the mask for size 8
* is 111) was already completely explored, it will not be visited again
* as we are sure that, we tried for example, both 0111 and 1111 (all the
* variations of the higher bit) so we don't need to test it again.
*
* WAIT... YOU HAVE *TWO* TABLES DURING REHASHING!
* �ȵȡ������� rehash ��ʱ����ǻ����������ϣ��İ���
*
* Yes, this is true, but we always iterate the smaller one of the tables,
* testing also all the expansions of the current cursor into the larger
* table. So for example if the current cursor is 101 and we also have a
* larger table of size 16, we also test (0)101 and (1)101 inside the larger
* table. This reduces the problem back to having only one table, where
* the larger one, if exists, is just an expansion of the smaller one.
*
* LIMITATIONS
* ����
*
* This iterator is completely stateless, and this is a huge advantage,
* including no additional memory used.
* �������������ȫ��״̬�ģ�����һ���޴�����ƣ�
* ��Ϊ���������ڲ�ʹ���κζ����ڴ������½��С�
*
* The disadvantages resulting from this design are:
* �����Ƶ�ȱ�����ڣ�
*
* 1) It is possible that we return duplicated elements. However this is usually
*    easy to deal with in the application level.
*    �������ܻ᷵���ظ���Ԫ�أ��������������Ժ�������Ӧ�ò�����
* 2) The iterator must return multiple elements per call, as it needs to always
*    return all the keys chained in a given bucket, and all the expansions, so
*    we are sure we don't miss keys moving.
*    Ϊ�˲�����κ�Ԫ�أ�
*    ��������Ҫ���ظ���Ͱ�ϵ����м���
*    �Լ���Ϊ��չ��ϣ��������������±�
*    ���Ե�����������һ�ε����з��ض��Ԫ�ء�
* 3) The reverse cursor is somewhat hard to understand at first, but this
*    comment is supposed to help.
*    ���α���з�ת��reverse����ԭ�������ȥ�Ƚ�������⣬
*    �����Ķ����ע��Ӧ�û�����������
*/
unsigned long dictScan(dict *d,
	unsigned long v,
	dictScanFunction *fn,
	void *privdata)
{
	dictht *t0, *t1;
	const dictEntry *de;
	unsigned long m0, m1;

	// �������ֵ�
	if (dictSize(d) == 0) return 0;

	// ����ֻ��һ����ϣ����ֵ�
	if (!dictIsRehashing(d)) {

		// ָ���ϣ��
		t0 = &(d->ht[0]);

		// ��¼ mask
		m0 = t0->sizemask;

		/* Emit entries at cursor */
		// ָ���ϣͰ
		de = t0->table[v & m0];
		// ����Ͱ�е����нڵ�
		while (de) {
			fn(privdata, de);
			de = de->next;
		}

		// ������������ϣ����ֵ�
	}
	else {

		// ָ��������ϣ��
		t0 = &d->ht[0];
		t1 = &d->ht[1];

		/* Make sure t0 is the smaller and t1 is the bigger table */
		// ȷ�� t0 �� t1 ҪС
		if (t0->size > t1->size) {
			t0 = &d->ht[1];
			t1 = &d->ht[0];
		}

		// ��¼����
		m0 = t0->sizemask;
		m1 = t1->sizemask;

		/* Emit entries at cursor */
		// ָ��Ͱ��������Ͱ�е����нڵ�
		de = t0->table[v & m0];
		while (de) {
			fn(privdata, de);
			de = de->next;
		}

		/* Iterate over indices in larger table that are the expansion
		* of the index pointed to by the cursor in the smaller table */
		// Iterate over indices in larger table             // ��������е�Ͱ
		// that are the expansion of the index pointed to   // ��ЩͰ�������� expansion ��ָ��
		// by the cursor in the smaller table               //
		do {
			/* Emit entries at cursor */
			// ָ��Ͱ��������Ͱ�е����нڵ�
			de = t1->table[v & m1];
			while (de) {
				fn(privdata, de);
				de = de->next;
			}

			/* Increment bits not covered by the smaller mask */
			v = (((v | m0) + 1) & ~m0) | (v & m0);

			/* Continue while bits covered by mask difference is non-zero */
		} while (v & (m0 ^ m1));
	}

	/* Set unmasked bits so incrementing the reversed cursor
	* operates on the masked bits of the smaller table */
	v |= ~m0;

	/* Increment the reverse cursor */
	v = rev(v);
	v++;
	v = rev(v);

	return v;
}

/* ------------------------- private functions ------------------------------ */

/*
* ������Ҫ����ʼ���ֵ䣨�Ĺ�ϣ�������߶��ֵ䣨�����й�ϣ��������չ
*
* T = O(N)
*/
static int _dictExpandIfNeeded(dict *d)
{
	// ����ʽ rehash �Ѿ��ڽ����ˣ�ֱ�ӷ���
	if (dictIsRehashing(d)) return DICT_OK;

	// ����ֵ䣨�� 0 �Ź�ϣ��Ϊ�գ���ô���������س�ʼ����С�� 0 �Ź�ϣ��
	// T = O(1)
	if (d->ht[0].size == 0) return dictExpand(d, DICT_HT_INITIAL_SIZE);

	// һ����������֮һΪ��ʱ�����ֵ������չ
	// 1���ֵ���ʹ�ýڵ������ֵ��С֮��ı��ʽӽ� 1��1
	//    ���� dict_can_resize Ϊ��
	// 2����ʹ�ýڵ������ֵ��С֮��ı��ʳ��� dict_force_resize_ratio
	if (d->ht[0].used >= d->ht[0].size &&
		(dict_can_resize ||
		d->ht[0].used / d->ht[0].size > dict_force_resize_ratio))
	{
		// �¹�ϣ��Ĵ�С������Ŀǰ��ʹ�ýڵ���������
		// T = O(N)
		return dictExpand(d, d->ht[0].used * 2);
	}

	return DICT_OK;
}

/*
* �����һ�����ڵ��� size �� 2 �� N �η���������ϣ���ֵ
*
* T = O(1)
*/
static unsigned long _dictNextPower(unsigned long size)
{
	unsigned long i = DICT_HT_INITIAL_SIZE;

	if (size >= LONG_MAX) return LONG_MAX;
	while (1) {
		if (i >= size)
			return i;
		i *= 2;
	}
}

/*
* ���ؿ��Խ� key ���뵽��ϣ�������λ��
* ��� key �Ѿ������ڹ�ϣ����ô���� -1
*
* ע�⣬����ֵ����ڽ��� rehash ����ô���Ƿ��� 1 �Ź�ϣ���������
* ��Ϊ���ֵ���� rehash ʱ���½ڵ����ǲ��뵽 1 �Ź�ϣ��
*
* T = O(N)
*/
static int _dictKeyIndex(dict *d, const void *key)
{
	unsigned int h, idx, table;
	dictEntry *he;

	if (_dictExpandIfNeeded(d) == DICT_ERR)
		return -1;

	h = dictHashKey(d, key);      // ���� key �Ĺ�ϣֵ
	// T = O(1)
	for (table = 0; table <= 1; table++) {
		idx = h & d->ht[table].sizemask;

		he = d->ht[table].table[idx];
		while (he) {
			if (dictCompareKeys(d, key, he->key))
				return -1;
			he = he->next;
		}

		// ������е�����ʱ��˵�� 0 �Ź�ϣ�������нڵ㶼������ key
		// �����ʱ rehahs ���ڽ��У���ô������ 1 �Ź�ϣ����� rehash
		if (!dictIsRehashing(d)) break; // ��������ǵ����̰汾����?ʱʱ�̶̿�������rehashing
	}

	return idx;
}

/*
* ����ֵ��ϵ����й�ϣ��ڵ㣬�������ֵ�����
*
* T = O(N)
*/
void dictEmpty(dict *d, void(callback)(void*)) {

	// ɾ��������ϣ���ϵ����нڵ�
	// T = O(N)
	_dictClear(d, &d->ht[0], callback);
	_dictClear(d, &d->ht[1], callback);
	// �������� 
	d->rehashidx = -1;
	d->iterators = 0;
}

/*
* �����Զ� rehash
*
* T = O(1)
*/
void dictEnableResize(void) {
	dict_can_resize = 1;
}

/*
* �ر��Զ� rehash
*
* T = O(1)
*/
void dictDisableResize(void) {
	dict_can_resize = 0;
}


unsigned int dictSdsCaseHash(const void *key) {
	//return dictGenCaseHashFunction((unsigned char*)key, strlen((char*)key));
	return atoi((char*)key);
}

int main(int argc, char* argv[])
{
	dictType commandTableDictType = {
		dictSdsCaseHash,           /* hash function */
		NULL,                      /* key dup */
		NULL,                      /* val dup */
		NULL,     /* key compare */
		NULL,         /* key destructor */
		NULL                       /* val destructor */
	};

	dict* dt = dictCreate(&commandTableDictType, NULL);
	dictAdd(dt, "1", "val1");
	dictAdd(dt, "2", "val2");
	dictAdd(dt, "3", "val3");
	dictAdd(dt, "4", "val4");

	dictAdd(dt, "5", "val5");
	dictAdd(dt, "6", "val6");
	dictAdd(dt, "7", "val7");
	dictAdd(dt, "8", "val8");
	dictAdd(dt, "9", "val9");
	dictAdd(dt, "10", "val10");
	dictAdd(dt, "11", "val11");
	dictAdd(dt, "12", "val12");
	dictAdd(dt, "13", "val13");
	dictAdd(dt, "14", "val14");
	dictAdd(dt, "15", "val15");
	dictAdd(dt, "16", "val16");

	dictAdd(dt, "17", "val17");
	dictAdd(dt, "18", "val18");

	dictEntry* en = dictFind(dt, "5");
	en = dictFind(dt, "key");
	return 0;
}

