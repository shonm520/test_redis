// test_ziplist.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include <stdlib.h>
#include <string.h>
#include <assert.h>


#define ZIP_END 255

#define ZIPLIST_HEAD 0
#define ZIPLIST_TAIL 1
#define ZIP_BIGLEN 254
#define ZIP_STR_06B (0 << 6)
#define ZIP_STR_MASK 0xc0
#define uchar unsigned char

#pragma pack(1)
struct ZipList  {
	unsigned int zlbytes;
	unsigned int zltail;
	unsigned short zllen;
	uchar* getEntryHead()  {
		return (uchar*)(this + 1);
	}
	uchar* getEntryEnd()  {
		return (uchar*)((uchar*)(this) + zlbytes - 1);
	}
};
#pragma pack()

typedef struct zlentry {
	unsigned int prevrawlensize, prevrawlen;
	unsigned int lensize, len;
	unsigned int headersize;       /* ��ǰ�ڵ� header �Ĵ�С, ���� prevrawlensize + lensize */
	unsigned char encoding;
	unsigned char *p;
} zlentry;

ZipList *ziplistNew(void);
ZipList *ziplistPush(ZipList* zl, unsigned char *s, unsigned int slen, int where);
unsigned char *ziplistIndex(ZipList* zl, int index);
unsigned char *ziplistNext(ZipList* zl, unsigned char *p);
unsigned char *ziplistPrev(uchar *zl, uchar *p);
unsigned int ziplistGet(uchar *p, uchar **sval, unsigned int *slen, long long *lval);
ZipList *ziplistInsert(ZipList*zl, uchar *p, uchar *s, unsigned int slen);
ZipList *ziplistDelete(ZipList *zl, uchar **p);
uchar *ziplistDeleteRange(uchar *zl, unsigned int index, unsigned int num);
unsigned int ziplistCompare(uchar *p, uchar *s, unsigned int slen);
uchar *ziplistFind(uchar *p, uchar *vstr, unsigned int vlen, unsigned int skip);
unsigned int ziplistLen(uchar *zl);

int zipPrevEncodeLength(uchar*, int len);
int zipEncodeLength(uchar*, int encoding, int len);
uchar *ziplistResize(uchar *zl, unsigned int len);
unsigned int zipRawEntryLength(uchar *p);
ZipList *__ziplistDelete(ZipList *zl, uchar *p, unsigned int num);
ZipList* __ziplistInsert(ZipList* zl, uchar* p, uchar* s, unsigned int slen);

int zipPrevLenByteDiff(uchar *p, unsigned int len);
ZipList *__ziplistCascadeUpdate(ZipList* zl, uchar* p);
void zipPrevEncodeLengthForceLarge(uchar*p, unsigned int len);

zlentry zipEntry(uchar* p);

uchar* ptrMoveOffset(void* ptr, int offset)  {
	return (uchar*)ptr + offset;
}

int getOffsetPtr(void* p1, void* p2)  {
	return (uchar*)p1 - (uchar*)p2;
}

ZipList* ziplistNew()  {
	unsigned int bytes = sizeof(ZipList)+1;
	ZipList* zlist = (ZipList*)malloc(bytes);
	zlist->zlbytes = bytes;
	zlist->zltail = sizeof(ZipList);
	zlist->zllen = 0;
	*(uchar*)(zlist + 1) = ZIP_END;
	return zlist;
}


int hashTypeSet(ZipList* zlist, char* field, char* value)  {
	int updata = 0;
	uchar* fptr = ziplistIndex(zlist, 0);
	if (fptr != NULL)  {
		fptr = ziplistFind(fptr, (uchar*)field, strlen(field), 1);
		if (fptr != NULL) {
			uchar* vptr = ziplistNext(zlist, fptr);    // ��λ�����ֵ
			assert(vptr != NULL);
			updata = 1;      
			zlist = ziplistDelete(zlist, &vptr);       // ɾ���ɵļ�ֵ��
			zlist = ziplistInsert(zlist, vptr, (uchar*)value, strlen(value));   //��ɾ���ĵط������value
		}
	}
	if (!updata)  {
		zlist = ziplistPush(zlist, (uchar*)field, strlen(field), 1);
		ziplistPush(zlist, (uchar*)value, strlen(value), 1);
	}

	return updata;
}

ZipList *ziplistInsert(ZipList *zl, uchar *p, uchar *s, unsigned int slen) {
	return __ziplistInsert(zl, p, s, slen);
}

ZipList* __ziplistInsert(ZipList* zl, uchar* p, uchar* s, unsigned int slen)  {
	int curLen = zl->zlbytes;
	int encoding = 0;
	int prevLen = 0;

	if (p[0] != ZIP_END)  {
		zlentry entry = zipEntry(p);
		prevLen = entry.prevrawlen;
	}
	else  {   //��β������
		uchar *ptail = (uchar*)zl + zl->zltail;
		if (ptail[0] != ZIP_END) {
			prevLen = zipRawEntryLength(ptail);
		}
	}
	int reqlen = slen;
	reqlen += zipPrevEncodeLength(NULL, prevLen);        //����pre_entry_length�ĳ���
	reqlen += zipEncodeLength(NULL, encoding, slen);     //����encoding��length�ĳ���

	int nextdiff = (p[0] != ZIP_END) ? zipPrevLenByteDiff(p, reqlen) : 0;

	int offset = getOffsetPtr(p, zl); // p - (uchar*)zl;
	zl = (ZipList*)ziplistResize((uchar*)zl, zl->zlbytes + reqlen);
	p = ptrMoveOffset(zl, offset);   // (()uchar*)zl + offset;
	if (p[0] != ZIP_END)  {
		memmove(p + reqlen, p - nextdiff, curLen - offset - 1 + nextdiff);
		zipPrevEncodeLength(p + reqlen, reqlen);    //��Ʈ�ƺ�ڵ��prevLen��ΪƮ��λ�Ƶĳ���
		zl->zltail = zl->zltail + reqlen;
		zlentry tail = zipEntry(p + reqlen);
		if (p[reqlen + tail.headersize + tail.len] != ZIP_END) {
			zl->zltail = zl->zltail + nextdiff;
		}
	}
	else  {
		zl->zltail = getOffsetPtr(p, zl);  //  p - (uchar*)zl;
	}

	//��ʼдentry
	int off = zipPrevEncodeLength(p, prevLen);
	p = ptrMoveOffset(p, off);
	off = zipEncodeLength(p, encoding, slen);
	p = ptrMoveOffset(p, off);
	if (encoding == 0)  {
		memcpy(p, s, slen);
	}
	zl->zllen = zl->zllen + 1;
	return zl;
}

ZipList* ziplistPush(ZipList* zl, uchar* s, unsigned int slen, int where)  {
	uchar* p = (where == ZIPLIST_HEAD) ? zl->getEntryHead() : zl->getEntryEnd();
	return __ziplistInsert(zl, p, s, slen);
}

ZipList *ziplistDelete(ZipList *zl, uchar **p) {

	/* ��Ϊ __ziplistDelete ʱ��� zl �����ڴ��ط���
	* ���ڴ�������ܻ�ı� zl ���ڴ��ַ
	* ����������Ҫ��¼���� *p ��ƫ����
	* ������ɾ���ڵ�֮��Ϳ���ͨ��ƫ�������� *p ��ԭ����ȷ��λ�� */
	size_t offset = *p - (uchar*)zl;
	zl = __ziplistDelete(zl, *p, 1);
	*p = (uchar*)zl + offset;
	return zl;
}

ZipList *__ziplistDelete(ZipList *zl, uchar *p, unsigned int num) {
	zlentry first = zipEntry(p);
	unsigned int deleted = 0;
	for (int i = 0; p[0] != ZIP_END && i < num; i++) {
		p += zipRawEntryLength(p);
		deleted++;
	}
	unsigned int totlen = p - first.p;    /* totlen�����б�ɾ���ڵ��ܹ�ռ�õ��ڴ��ֽ��� */
	if (totlen > 0) {
		int nextdiff = 0;
		if (p[0] != ZIP_END) {
			/* ִ�е�����,��ʾɾ���ڵ�֮����Ȼ�нڵ���� */
			/* ��Ϊλ�ڱ�ɾ����Χ֮��ĵ�һ���ڵ�� header ���ֵĴ�С
			* �������ɲ����µ�ǰ�ýڵ㣬������Ҫ�����¾�ǰ�ýڵ�֮����ֽ�����
			* T = O(1) */
			nextdiff = zipPrevLenByteDiff(p, first.prevrawlen);
			
			p -= nextdiff;                                /* �������Ҫ�Ļ�����ָ�� p ���� nextdiff �ֽڣ�Ϊ�� header �ճ��ռ� */
			
			zipPrevEncodeLength(p, first.prevrawlen);    /* �� first ��ǰ�ýڵ�ĳ��ȱ����� p �� */

			zl->zltail = zl->zltail - totlen;

			zlentry tail = zipEntry(p);
			if (p[tail.headersize + tail.len] != ZIP_END) {
				zl->zltail = zl->zltail + nextdiff;
			}
			memmove(first.p, p, (zl->zlbytes - (p - (uchar*)zl) - 1));  /* �ӱ�β���ͷ�ƶ����ݣ����Ǳ�ɾ���ڵ������ */
		}
		else {
			zl->zltail = (first.p - (uchar*)zl) - first.prevrawlen;
		}
		
		size_t offset = first.p - (uchar*)zl;   /* ��С������ziplist�ĳ��� */
		zl = (ZipList*)ziplistResize((uchar*)zl, zl->zlbytes - totlen + nextdiff);
		zl->zllen -= deleted;

		p = (uchar*)zl + offset;
		/* ��� p ��ָ��Ľڵ�Ĵ�С�Ѿ��������ô���м�������
		* ��� p ֮������нڵ��Ƿ���� ziplist �ı���Ҫ�� */
		if (nextdiff != 0)
			zl = __ziplistCascadeUpdate(zl, p);
	}
	return zl;
}

ZipList *__ziplistCascadeUpdate(ZipList* zl, uchar* p) {
	size_t curlen = zl->zlbytes, rawlen, rawlensize;
	size_t offset, noffset, extra;
	uchar *np;
	zlentry cur, next;

	while (p[0] != ZIP_END) {
		cur = zipEntry(p);
		rawlen = cur.headersize + cur.len;
		rawlensize = zipPrevEncodeLength(NULL, rawlen);
		if (p[rawlen] == ZIP_END) break;
		next = zipEntry(p + rawlen);
		if (next.prevrawlen == rawlen) break;
		if (next.prevrawlensize < rawlensize) {
			offset = p - (uchar*)zl;
			extra = rawlensize - next.prevrawlensize;
			zl = (ZipList*)ziplistResize((uchar*)zl, curlen + extra);
			p = (uchar*)zl + offset;
			np = p + rawlen;
			noffset = np - (uchar*)zl;
			if (((uchar*)zl + zl->zltail) != np) {
				zl->zltail = zl->zltail + extra;
			}
			memmove(np + rawlensize, np + next.prevrawlensize, curlen - noffset - next.prevrawlensize - 1);
			zipPrevEncodeLength(np, rawlen);
			p += rawlen;
			curlen += extra;
		}
		else {
			if (next.prevrawlensize > rawlensize) {
				/* ִ�е����˵�� next �ڵ����ǰ�ýڵ�� header �ռ��� 5 �ֽ�
				* ������ rawlen ֻ��Ҫ 1 �ֽ�
				* ���ǳ��򲻻�� next ������С��
				* ��������ֻ�� rawlen д�� 5 �ֽڵ� header �о����ˡ�
				* T = O(1) */
				zipPrevEncodeLengthForceLarge(p + rawlen, rawlen);
			}
			else {
				/* ���е����
				* ˵�� cur �ڵ�ĳ������ÿ��Ա��뵽 next �ڵ�� header ��
				* T = O(1) */
				zipPrevEncodeLength(p + rawlen, rawlen);
			}

			/* Stop here, as the raw length of "next" has not changed. */
			break;
		}
	}
	return zl;
}

void zipPrevEncodeLengthForceLarge(uchar*p, unsigned int len) {
	if (p == NULL) return;

	/* ����5�ֽڵĳ��ȱ�־ */
	p[0] = ZIP_BIGLEN;
	/* д��len */
	memcpy(p + 1, &len, sizeof(len));
}


int zipPrevLenByteDiff(uchar *p, unsigned int len) {
	/* �����ٸ����Ӱ�,�������ǰ�ýڵ���Ҫ5���ֽ�,�����뵱ǰ�ڵ���Ҫ1���ֽ�,��ô����ֵ��4 */
	unsigned int prevlensize;
	/* ȡ������ԭ����ǰ�ýڵ㳤��������ֽ��� */

	prevlensize = (p[0] < ZIP_BIGLEN ? 1 : 5);
	
	/* �������len������ֽ���,Ȼ����м������� */
	return zipPrevEncodeLength(NULL, len) - prevlensize;
}

int zipPrevEncodeLength(uchar* p, int len)  {
	if (p == NULL) {
		return 1;
	}
	else {
		p[0] = len;
		return 1;
	}
}

int zipEncodeLength(uchar* p, int encoding, int rawlen)  {
	uchar len = 1;
	uchar buf[5];
	if (encoding == 0)  {     //�ַ���
		if (rawlen < 0x3f)  {
			if (!p) return len;
			buf[0] = ZIP_STR_06B | rawlen; 
		}
	}
	memcpy(p, buf, len);
	return len;
}

uchar *ziplistResize(uchar *zl, unsigned int len) {
	zl = (uchar*)realloc(zl, len);
	((ZipList*)zl)->zlbytes = len;
	zl[len - 1] = ZIP_END;
	return zl;
}

uchar* ziplistIndex(ZipList* zl, int index)  {
	uchar* p;
	if (index < 0)  {

	}
	else  {
		p = zl->getEntryHead();
		while (p[0] != ZIP_END && index--)  {
			p += zipRawEntryLength(p);
		}
	}
	return (p[0] == ZIP_END || index > 0) ? NULL : p;
}

int hashTypeGet(ZipList* zl, char* field, uchar** vstr, unsigned int* vlen)  {
	uchar* fptr = ziplistIndex(zl, 0);
	uchar* vptr = NULL;
	if (fptr != NULL)  {
		fptr = ziplistFind(fptr, (uchar*)field, strlen((char*)field), 1);
		if (fptr != NULL)  {
			vptr = ziplistNext(zl, fptr);
		}
	}
	if (vptr != NULL)  {
		ziplistGet(vptr, vstr, vlen, NULL);
		return 0;
	}
	return -1;
}

uchar *ziplistNext(ZipList* zl, uchar *p) {
	if (p[0] == ZIP_END) {      /* p �Ѿ�ָ���б�ĩ�� */
		return NULL;
	}

	p += zipRawEntryLength(p);    /* ָ���һ�ڵ� */
	if (p[0] == ZIP_END) {
		return NULL;
	}
	return p;
}

unsigned int zipRawEntryLength(uchar *p)  {
	zlentry entry = zipEntry(p);
	return entry.prevrawlensize + entry.lensize + entry.len;
}

uchar *ziplistFind(uchar *p, uchar *vstr, unsigned int vlen, unsigned int skip) {
	int skipcnt = 0;
	uchar vencoding = 0;
	long long vll = 0;

	while (p[0] != ZIP_END) {     /* ֻҪδ�����б�ĩ�ˣ���һֱ���� */
		zlentry entry = zipEntry(p);
		uchar* q = p + entry.headersize;

		if (skipcnt == 0) {
			if (entry.encoding == 0) {
				if (entry.len == vlen && memcmp(q, vstr, vlen) == 0) {
					return p;
				}
			}
			skipcnt = skip;
		}
		else {
			skipcnt--;
		}
		p = q + entry.len;      /* ����ָ�룬ָ����ýڵ� */
	}
	return NULL;                /* û���ҵ�ָ���Ľڵ� */
}

unsigned int ziplistGet(uchar *p, uchar **sstr, unsigned int *slen, long long *sval)  {
	if (p == NULL || p[0] == ZIP_END) return 0;
	if (sstr) *sstr = NULL;

	zlentry entry = zipEntry(p);
	if (entry.encoding == 0) {    /* �ڵ��ֵΪ�ַ��������ַ������ȱ��浽 *slen ���ַ������浽 *sstr */
		if (sstr) {
			*slen = entry.len;
			*sstr = p + entry.headersize;
		}
	}
	return 1;
}

zlentry zipEntry(uchar* p)  {
	zlentry e;
	e.prevrawlensize = (p[0] < ZIP_BIGLEN ? 1 : 5);
	if (e.prevrawlensize == 1) {
		e.prevrawlen = p[0];
	}

	e.encoding = *(p + e.prevrawlensize);
	if (e.encoding < ZIP_STR_MASK)  {
		e.encoding &= ZIP_STR_MASK;
	}

	if (e.encoding == 0)  {       //���ݱ�����len���ֽڳ��Ⱥ�len��ֵ
		e.lensize = 1;
		e.len = *(p + e.prevrawlensize) & 0x3F;
	}
	e.headersize = e.prevrawlensize + e.lensize;
	e.p = p;
	return e; 
}



int main(int argc, char* argv[])
{
	ZipList* zl = ziplistNew();
	hashTypeSet(zl, "name", "shonm");
	hashTypeSet(zl, "addr", "shenzh");


	hashTypeSet(zl, "name", "shonm2");

	unsigned int len = 0;
	uchar* ret = NULL;
	hashTypeGet(zl, "name", &ret, &len);
	return 0;
}

