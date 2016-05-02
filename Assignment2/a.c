#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char u8;	//1字节
typedef unsigned short u16;	//2字节
typedef unsigned int u32;	//4字节

int BytePerSec;	//每个扇区的字节数
int SecPerClus;	//每个簇的扇区数
int NumResSec;	//保留扇区数目(引导扇区)--1
int NumFATs;	//FAT表数目
int RootEntCnt;	//根目录最大文件数
int TolSecCnt;	//总扇区数
int SecPerFAT;	//每个FAT的扇区数
char result[20][50];	//保存结果
int ptr;	//结果中的指针
int clusNum[20];	//打印文件的簇号

#pragma pack(1)

//引导盘信息
struct BPB {
	u16 BPB_BytePerSec;
	u8 BPB_SecPerClus;
	u16 BPB_RsvdSecCnt;
	u8 BPB_NumFATs;
	u16 BPB_RootEntCnt;
	u16 BPB_TolSecCnt;
	u8 BPB_Ign;
	u16 BPB_SecPerFAT;
	u16 BPB_SecPerTrk;
	u16 BPB_NumHeads;
};

//根目录中的文件条目
struct DirEntry {
	char Dir_Name[11];
	u8 Dir_Attr;
	char reserved[14];	//包含很多字段，不重要
	u16 Dir_FirClus;
	u32 Dir_FileSize;
};

#pragma pack()

//方法申明
void init(FILE* file, struct BPB* bpb_ptr);	//初始化全局变量
void printFiles(FILE* file, struct DirEntry* rootEntry_ptr);	//打印所有文件结构
void printSubFiles(FILE* file, char* dir, int startClus);	//打印子文件
int getFATValue(FILE* file, int num);	//读取FAT值
void countFiles(FILE* file, char* addr);	//响应count指令
void collect(char* name, int clus);	//收集结果
void findFiles(FILE* file, char* tempAddr);	//找到目的文件并读取
void printByAsm(char* name);	//通过汇编输出
void printtxt(FILE* file, int clus);	//输出文件内容
int endWithTxt(char* s);	//返回是否是文本文件
void count(char* addr);	//分析结构
void print(char* content, int length, int color);	//extern

int main() {
	FILE* file;
	file = fopen("abc.img", "rb");	//以二进制读取形式打开

	struct BPB bpb;
	struct BPB* bpb_ptr = &bpb;
	//初始化全局变量
	init(file, bpb_ptr);
	//打印结构
	struct DirEntry rootEntry;
	struct DirEntry* rootEntry_ptr = &rootEntry;
	ptr = 0;
	printFiles(file, rootEntry_ptr);
	int x;
	for (x = 0; x < ptr; x++) {
		printByAsm(result[x]);
	}
	// 两种指令
	while(1) {
		char* input = (char*)malloc(50);
		scanf("%s" ,input);
		int toCount = 1;	//是否执行count命令
		char* target = "count";
		int i;
		if (strcmp(input, target)==0) {
			scanf("%s", input);
			countFiles(file, input);
		} else {
			findFiles(file, input);
		}
		free(input);
	}
}

//读取引导盘，初始化全局变量
void init(FILE* file, struct BPB* bpb_ptr) {
	//前11字节省略
	fseek(file, 11, SEEK_SET);
	//将信息填入
	fread(bpb_ptr, 1, 17, file);
	BytePerSec = bpb_ptr->BPB_BytePerSec;
	SecPerClus = bpb_ptr->BPB_SecPerClus;
	NumResSec = bpb_ptr->BPB_RsvdSecCnt;
	NumFATs = bpb_ptr->BPB_NumFATs;
	RootEntCnt = bpb_ptr->BPB_RootEntCnt;
	TolSecCnt = bpb_ptr->BPB_TolSecCnt;
	SecPerFAT = bpb_ptr->BPB_SecPerFAT;
}

void printFiles(FILE* file, struct DirEntry* rootEntry_ptr) {
	int base = (NumResSec + NumFATs * SecPerFAT) * BytePerSec;	//根目录其实地址
	char* name = (char*)malloc(13);	//文件名8字节，扩展名3字节，点1字节

	int i;
	for (i = 0; i < RootEntCnt; i++) {
		fseek(file, base, SEEK_SET);
		fread(rootEntry_ptr, 1, 32, file);
		base+=32;

		if(rootEntry_ptr->Dir_Name[0] == '\0') continue;	//编译器报告0xE5不需要比较
		// else if(rootEntry_ptr->Dir_Name[0] == 0x00) break;

		int isFile = 0;	//判断是否为合理文件
		int j;
		for (j = 0; j < 11; j++) {
			if (!(((rootEntry_ptr->Dir_Name[j] >= 48) && (rootEntry_ptr->Dir_Name[j] <= 57)) ||
				((rootEntry_ptr->Dir_Name[j] >= 65) && (rootEntry_ptr->Dir_Name[j] <= 90)) ||
					((rootEntry_ptr->Dir_Name[j] >= 97) && (rootEntry_ptr->Dir_Name[j] <= 122)) ||
						(rootEntry_ptr->Dir_Name[j] == ' '))) {
				isFile = 1;	//文件名中包含非法字符
				break;
			}
		}
		if(isFile == 1) continue;

		int k;
		if ((rootEntry_ptr->Dir_Attr&0x10) == 0) {	//文件
			int temp = 0;
			for (k = 0; k < 11; k++) {
				if (rootEntry_ptr->Dir_Name[k] != 0x20) {
					name[temp] = rootEntry_ptr->Dir_Name[k];
					temp++;
				}
				if (k == 7) {
					name[temp] = '.';
					temp++;
				}
			}
			name[temp] = '\0';	//字符串结尾
			collect(name, rootEntry_ptr->Dir_FirClus);
		} else {	//目录
			int temp = 0;
			for (k = 0; k < 11; k++) {
				if (rootEntry_ptr->Dir_Name[k] != 0x20) {
					name[temp] = rootEntry_ptr->Dir_Name[k];
					temp++;
				} else {
					name[temp] = '\0';
					break;
				}
			}
			//输出子目录
			printSubFiles(file, name, rootEntry_ptr->Dir_FirClus);
		}
	}
	free(name);
}

//打印目录子文件
void printSubFiles(FILE* file, char* name, int firClus) {
	int base = (NumResSec + NumFATs * SecPerFAT + (RootEntCnt * 32 + BytePerSec - 1) / BytePerSec) * BytePerSec;	//数据区开始
	char* filename = (char*)malloc(30);	//保存完整文件名
	int length = strlen(name);
	strcpy(filename, name);
	filename[length] = '/';
	length++;
	filename[length] = '\0';
	char* newPart = &filename[length];

	int currentClus = firClus;
	int value = 0;
	int isEmptyDirectory = 0;	//判断是否为空目录
	while (value < 0xFF7) {	//坏簇或文件最后一个簇
		value = getFATValue(file, currentClus);
		char* str = (char*)malloc(SecPerClus * BytePerSec);	//开辟一块用来保存一个簇内容的空间
		char* content = str;

		if (value == 0x00) break;	//遇到0x00,剩下的都为空

		int startByte = base + (currentClus - 2) * SecPerClus * BytePerSec;
		fseek(file, startByte, SEEK_SET);
		fread(content, 1, SecPerClus * BytePerSec, file);
		int count = SecPerClus * BytePerSec;
		int times = 0;
		while (times < count) {
			int i;
			char temp[13];
			if (content[times] == '\0') {
				times += 32;
				continue;
			}
			int j;
			int flag = 0;	//文件名内不存在非法字符
			for (j = times; j < times+11; j++) {
				if (!(((content[j] >= 48) && (content[j] <= 57)) || ((content[j] >= 65) && (content[j] <= 90)) 
					|| ((content[j] >= 97) && (content[j] <= 122)) || (content[j] == ' '))) {
					flag = 1;
					break;
				}
			}
			if (flag == 1) {
				times += 32;
				continue;
			}
			isEmptyDirectory = 1;	//非空目录
			//判断子目录还是子文件
			int k;
			int tempIndex = 0;
			if ((content[times+11]&0x10) == 0) {	//是文件
				for (k = 0; k < 11; k++) {
					if (content[times+k] != 0x20) {
						temp[tempIndex] = content[times+k];
						tempIndex++;
					}
					if (k == 7) {
						temp[tempIndex] = '.';
						tempIndex++;
					}
				}
				temp[tempIndex] = '\0';
				strcpy(newPart, temp);
				newPart[strlen(temp)] = '\0';
				collect(filename, content[times+26] + content[times+27] * 8);
			} else {	//目录
				for (k = 0; k < 8; k++) {
					if (content[times+k] == 0x20) break;
					temp[tempIndex] = content[times+k];
					tempIndex++;
				}
				temp[tempIndex] = '\0';
				strcpy(newPart, temp);
				newPart[strlen(temp)] = '\0';
				int nextClus = content[times+26] + content[times+27] * 8;
				printSubFiles(file, filename, nextClus);
			}
			times+=32;
		}
		free(str);	//开辟的内存需要清理
		currentClus = value;
	};

	if (isEmptyDirectory == 0) {
		filename[length-1] = '\0';
		collect(filename, 0);
		// printSubFiles(file, newPart, currentClus);
	}
	free(filename);
}

int getFATValue(FILE* file, int clus) {
	int base = NumResSec * BytePerSec;
	int offset = base + clus * 3 / 2;
	int type = 0;
	if (clus % 2 == 0) {
		type = 0;	//从一个字节的中间开始
	} else {
		type = 1;	//从一个字节的开头开始
	}
	u16 bytes;
	u16* bytes_ptr = &bytes;
	fseek(file, offset, SEEK_SET);
	fread(bytes_ptr, 1, 2, file);
	if (type == 0) {
		return (bytes<<4)>>4;
	} else {
		return bytes>>4;
	}
}

void collect(char* name, int clus) {
	strcpy(result[ptr], name);
	clusNum[ptr] = clus;
	ptr++;
}

void countFiles(FILE* file, char* tempAddr) {
	int o;
	for (o = 0; o < strlen(tempAddr); o++) {
		if (tempAddr[o]>=97 && tempAddr[o] <= 122) {
			tempAddr[o] -= 32;
		}
	}
	char* start = tempAddr;
	int isExist = 1;
	if (endWithTxt(start)) {
		char* a = start + strlen(tempAddr);
		char* s = " is not a directory!";
		strcpy(a, s);
		printByAsm(start);
		return;
	}

	start[strlen(tempAddr)] = '\0';
	count(start);
}

void count(char* addr) {
	int i;
	char* pointer;	//	指针
	int fileCount = 0;	//文件数
	int dirCount = 0;	//目录数
	int indCount = 0;	//读取字符串中的'/'数
	char* array[20];	//子文件夹
	char name[20][30];
	char* ta[20];
	int indexa = 0;
	int indexn = 0;
	int indext = 0;
	for (i = 0; i < strlen(addr); i++) {
		if (addr[i]=='/') {
			indCount++;
		}
	}
	for (i = 0; i < ptr; i++) {
		int j;
		int belong = 1;
		if (strlen(addr) > strlen(result[i])) {
			continue;
		} else if(strcmp(addr, result[i])==0) {
			break;
		}
		for (j = 0; j < strlen(addr); j++) {
			if(addr[j] != result[i][j]) {
				belong = 0;
				break;
			}
		}
		if (!belong) {
			continue;
		} else {
			if (result[i][j] != '/') {
				continue;
			}
			array[indexa++] = result[i];
		}
		if (endWithTxt(result[i])) {
			fileCount++;
		}
	}
	int k;
	for (k = 0; k < indexa; k++) {
		array[k] = array[k] + strlen(addr) + 1;
		if (strchr(array[k], '/') == NULL) {
			if (!endWithTxt(array[k])) {
				strcpy(name[indexn], array[k]);
				indexn++;
			}
		} else {
			while (strchr(array[k], '/') != NULL) {	//字符串中存在'/'
				int l;
				int f = 1;
				char* r = (char*)malloc(30);
				strcpy(r, array[k]);
				strchr(r, '/')[0] = '\0';
				for (l = 0; l < indexn; l++) {
					if (strcmp(r, name[l]) == 0) {
						f = 0;
						break;
					}
				}
				if (f) {
					strcpy(name[indexn], r);
					indexn++;
				}
				array[k] = strchr(array[k], '/')+1;
				if (strchr(array[k], '/') == NULL && !endWithTxt(array[k])) {
					strcpy(name[indexn], array[k]);
					indexn++;
				}
				free(r);
			}
		}
	}
	dirCount = indexn;
	char* t = (char*)malloc(50);
	pointer = t;
	char* colon = ":";
	char* dir = "directory";
	char* file = "file";
	char* space = " ";
	for (i = 0; i < indCount; i++) {
		strcpy(pointer, space);
		pointer++;
	}
	char* at = addr;
	while (strchr(at, '/') != NULL) {
		at = strchr(at, '/');
		at++;
	}
	strcpy(pointer, at);
	pointer += strlen(at);
	strcpy(pointer, colon);
	pointer++;
	pointer[0] = fileCount + 48;
	pointer++;
	strcpy(pointer, space);
	pointer++;	
	strcpy(pointer, file);
	pointer+=4;
	strcpy(pointer, space);
	pointer++;
	pointer[0] = dirCount + 48;
	pointer++;
	strcpy(pointer, space);
	pointer++;
	strcpy(pointer, dir);
	pointer+=9;
	pointer[0] = '\0';
	printByAsm(t);
	free(t);

	//递归
	for (i = 0; i < ptr; i++) {
		ta[indext] = (char*)malloc(30);
		if (strcmp(result[i], addr) == 0) {
			return;
		}
		if (strlen(addr) > strlen(result[i])) {
			continue;
		}
		int flag = 1;
		for (k = 0; k < (int)strlen(addr); k++) {
			if (addr[k] != result[i][k]) {
				flag = 0;
				break;
			}
		}
		if (!flag) {
			continue;
		}
		flag = 1;
		int l;
		for (k = 0; k < indext; k++) {
			if (strlen(result[i]) < strlen(ta[k])) {
				continue;
			}
			int cmp = 1;
			for (l = 0; l < (int)strlen(ta[k]) && l < (int)strlen(result[i]); l++) {
				if (ta[k][l] != result[i][l]) {
					cmp = 0;
					break;
				}
			}
			if (cmp) {
				flag = 0;
				break;
			}
		}
		if (flag == 0) {
			continue;
		}
		if (strchr(result[i]+strlen(addr)+1, '/') == NULL && !endWithTxt(result[i])) {
			for (l = 0; l < strlen(result[i]); l++) {
				ta[indext][l] = result[i][l];
			}
			ta[indext][l] = '\0';
			indext++;
		} else if (strchr(result[i]+strlen(addr)+1, '/') == NULL && endWithTxt(result[i])) {	//文件
			continue;
		} else {
			char* end = strchr(result[i]+strlen(addr)+1, '/');
			for (l = 0; l < end-result[i]; l++) {
				ta[indext][l] = result[i][l];
			}
			ta[indext][l] = '\0';
			indext++;
		}
	}
	for (i = 0; i < indext; i++) {
		count(ta[i]);
	}
}

void findFiles(FILE* file, char* addr) {
	int isExist = 0;	//是否存在对象
	int o = 0;
	for (o = 0; o < strlen(addr); o++) {
		if (addr[o]>=97 && addr[o] <= 122) {
			addr[o] -= 32;
		}
	}
	if (endWithTxt(addr)) {	//是文件
		char* fileToPrint = addr;
		// printFiles(file, entry);
		int i;
		for (i = 0; i < ptr; i++) {
			if (strcmp(result[i], addr)==0) {
				isExist = 1;
				break;
			}
		}
		if (isExist) {
			printtxt(file, clusNum[i]);
		}
	} else {	//是目录
		char* temp1;
		if (addr[strlen(addr)-1] == '/') {
			int i;
			for (i = 0; i < ptr; i++) {
				int flag = 1;
				int j;
				for (j = 0; j < strlen(addr); j++) {
					if (result[i][j] != addr[j]) {
						flag = 0;
						break;
					}
				}
				if (flag) {
					isExist = 1;
					printByAsm(result[i]);
				}
			}
		}
	}
	if (!isExist) {
		printByAsm("Unknown file");
	}
}

void printtxt(FILE* file, int clus) {
	char* text = (char*)malloc(10000);
	char* temp;
	temp = text;
	int value = clus;
	int base = (NumResSec + NumFATs * SecPerFAT + (RootEntCnt * 32 + BytePerSec - 1) / BytePerSec) * BytePerSec;	//数据区开始
	while(value!=0xFF7) {
		char* str = (char*)malloc(SecPerClus * BytePerSec);	//开辟一块用来保存一个簇内容的空间
		char* content = str;
		int startByte = base + (value - 2) * SecPerClus * BytePerSec;
		fseek(file, startByte, SEEK_SET);
		fread(content, 1, SecPerClus * BytePerSec, file);
		int y;
		int flag = 0;
		for (y = 0; y < SecPerClus * BytePerSec; y++) {
			if (content[y]==0x00) {
				flag = 1;
				temp[0] = content[y];
				break;
			}
			temp[0] = content[y];
			temp++;
		}
		if (flag) {
			free(str);
			temp[0] = 0x20;
			break;
		}
		free(str);
		if (value>=0xFF8) {
			temp[0] = 0x20;
			break;
		}
		value = getFATValue(file, value);

	}
	printByAsm(text);
	free(text);
}

void printByAsm(char* name) {
	if (endWithTxt(name)) {
		char* p = name;
		while (strchr(p, '/')!=NULL) {
			p++;
		}
		char* temp = (char*)malloc(p-name);
		int i;
		for (i = 0; i < (int)(p-name); i++) {
			temp[i] = name[i];
		}
		print(temp, (int)(p - name), 1);
		print(p, (int)strlen(p), 0);	//文件，使用不同颜色
		print("\n", 1, 1);
		free(temp);
	} else {
		print(name, (int)strlen(name), 1);	//非文件，正常颜色
		print("\n", 1, 1);
	}
}

int endWithTxt(char* s) {
	int isFile = 1;	//是否是文件
	int i = 0;
	char* fileEnd = ".TXT";	//以".TXT"结尾，是文件
	for (i = 0; i < 4; i++) {
		if (fileEnd[i] != s[strlen(s)-4+i]) {
			isFile = 0;
			break;
		}
	}
	return isFile;
}