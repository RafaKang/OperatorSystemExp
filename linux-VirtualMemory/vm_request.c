/*
 * vm_request.c
 *
 *  Created on: May 27, 2015
 *      Author: kangshiyu
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/stat.h>
#include <time.h>
#include "vm_global.h"

int main()
{
	FILE *fp;
	int write_num, i = 0, n = 0;
	char c;
	printf("请求数量\n");
	scanf("%d",&n);
	srand(time(NULL));
	MemoryAccessRequestPtr MemRequest = (MemoryAccessRequestPtr) malloc (sizeof(MemoryAccessRequest));
	char *str = (char*) malloc (sizeof(MemoryAccessRequest) * n);
	char *p = str;
	int fd = open(FIFO,O_WRONLY);
	while (i < n)
	{
		//printf("%d\n",i);
		/*if ((fp = fopen(FIFO,"a+")) == NULL)
		{
			exit(1);
		}*/
		do_request(MemRequest);
		printf("请求成功　%d\n",++i);
		write_num = write(fd,MemRequest,sizeof(MemoryAccessRequest));//,1,fp);
		//memcpy(p,MemRequest,sizeof(MemoryAccessRequest));
		//p += sizeof(MemoryAccessRequest);
		//fclose(fp);
	}
	//fp = fopen(FIFO,"w");
	//fwrite(str,sizeof(MemoryAccessRequest)*n,1,fp);
	//fclose(fp);
	return 0;
}

void do_request(MemoryAccessRequestPtr MemRequest)
{
	//printf("输入R随机生成请求\n");
	MemRequest->virtual_addr = rand()%VIRTUAL_MEM_SIZE;
	
	switch(rand()%3)
	{
		case 0:
		{
			MemRequest->req_Type = READ;
			printf("请求读取:　%u地址\n",MemRequest->virtual_addr);
			break;
		}
		case 1:
		{
			MemRequest->req_Type = WRITE;
			MemRequest->value = rand()%MAX_VALUE;
			printf("请求写入: %u于%u地址\n",MemRequest->value,MemRequest->virtual_addr);
			break;
		}
		case 2:
		{
			MemRequest->req_Type = EXECUTE;
			printf("请求执行: %u地址\n",MemRequest->virtual_addr);
			break;
		}
	}
}

