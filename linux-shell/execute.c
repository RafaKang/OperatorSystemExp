#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/termios.h>
#include <glob.h>

#include "global.h"
#define DEBUG

const char fifo[20][40] = {"/tmp/myfifo0","/tmp/myfifo1","/tmp/myfifo2","/tmp/myfifo3","/tmp/myfifo4","/tmp/myfifo5","/tmp/myfifo6","/tmp/myfifo7","/tmp/myfifo8","/tmp/myfifo9","/tmp/myfifo10","/tmp/myfifo11","/tmp/myfifo12","/tmp/myfifo13","/tmp/myfifo14","/tmp/myfifo15","/tmp/myfifo16","/tmp/myfifo17","/tmp/myfifo18","/tmp/myfifo19"};
int goon = 0, ingnore = 0;       //用于设置signal信号量
char *envPath[10], cmdBuff[40];  //外部命令的存放路径及读取外部命令的缓冲空间
History history;                 //历史命令
Job *head = NULL;                //作业头指针
pid_t fgPid;                     //当前前台作业的进程号
int goon1 = 0;

/*******************************************************
                  工具以及辅助方法
********************************************************/
/*判断命令是否存在*/

char *FIFO(int x)
{
	return fifo[x];
}

int exists(char *cmdFile){
    int i = 0;
    if((cmdFile[0] == '/' || cmdFile[0] == '.') && access(cmdFile, F_OK) == 0){ //命令在当前目录
        strcpy(cmdBuff, cmdFile);
        return 1;
    }else{  //查找ysh.conf文件中指定的目录，确定命令是否存在
        while(envPath[i] != NULL){ //查找路径已在初始化时设置在envPath[i]中
            strcpy(cmdBuff, envPath[i]);
            strcat(cmdBuff, cmdFile);
            
            if(access(cmdBuff, F_OK) == 0){ //命令文件被找到
                return 1;
            }
            
            i++;
        }
    }
    
    return 0; 
}

/*将字符串转换为整型的Pid*/
int str2Pid(char *str, int start, int end){
    int i, j;
    char chs[20];
    
    for(i = start, j= 0; i < end; i++, j++){
        if(str[i] < '0' || str[i] > '9'){
            return -1;
        }else{
            chs[j] = str[i];
        }
    }
    chs[j] = '\0';
    
    return atoi(chs);
}

/*调整部分外部命令的格式*/
void justArgs(char *str){
    int i, j, len;
    len = strlen(str);
    
    for(i = 0, j = -1; i < len; i++){
        if(str[i] == '/'){
            j = i;
        }
    }

    if(j != -1){ //找到符号'/'
        for(i = 0, j++; j < len; i++, j++){
            str[i] = str[j];
        }
        str[i] = '\0';
    }
}

/*设置goon*/
void setGoon(){
    goon = 1;
	printf("get ! %d\n",getpid());
}

/*释放环境变量空间*/
void release(){
    int i;
    for(i = 0; strlen(envPath[i]) > 0; i++){
        free(envPath[i]);
    }
}

/*******************************************************
                  信号以及jobs相关
********************************************************/
/*添加新的作业*/
Job* addJob(pid_t pid){
    Job *now = NULL, *last = NULL, *job = (Job*)malloc(sizeof(Job));
    
	//初始化新的job
    job->pid = pid;
    strcpy(job->cmd, inputBuff);
    strcpy(job->state, RUNNING);
    job->next = NULL;
    
    if(head == NULL){ //若是第一个job，则设置为头指针
        head = job;
    }else{ //否则，根据pid将新的job插入到链表的合适位置
		now = head;
		while(now != NULL && now->pid < pid){
			last = now;
			now = now->next;
		}
        last->next = job;
        job->next = now;
    }
    
    return job;
}

/*移除一个作业*/
void rmJob(int sig, siginfo_t *sip, void* noused){
    pid_t pid;
    Job *now = NULL, *last = NULL;
    //printf("I'm here!\n");
    if (sip->si_code == CLD_CONTINUED)
	{
		setGoon();
		return;   
	} 
    goon1 = 1;
    if(ingnore == 1){
        ingnore = 0;
        return;
    }
    
    pid = sip->si_pid;
    //printf("%d\n",pid);
    now = head;
    if (now == NULL)
	//printf("shit!\n");
	while(now != NULL && now->pid < pid){
		last = now;
		now = now->next;
	}
    
    if(now == NULL){ //作业不存在，则不进行处理直接返回
        return;
    }
    //printf("remove!\n");
	//开始移除该作业
    if(now == head){
        head = now->next;
    }else{
        last->next = now->next;
    }    //printf("handled\n");
    wait(NULL);

    printf("not zushe in NULL!\n");
    free(now);
    //sleep(1);
    //printf("%d\n",goon1);
}

/*组合键命令ctrl+z*/
void ctrl_Z(){
    Job *now = NULL;
    
    if(fgPid == 0){ //前台没有作业则直接返回
        return;
    }
    
    //SIGCHLD信号产生自ctrl+z
    ingnore = 1;
    
	now = head;
	while(now != NULL && now->pid != fgPid)
		now = now->next;
    
    if(now == NULL){ //未找到前台作业，则根据fgPid添加前台作业
        now = addJob(fgPid);
    }
    
	//修改前台作业的状态及相应的命令格式，并打印提示信息
    strcpy(now->state, STOPPED); 
    int len = strlen(now->cmd);
    now->cmd[len] = '&';
    now->cmd[len + 1] = 0;
    printf("[%d]\t%s\t\t%s\n", now->pid, now->state, now->cmd);
    
	//发送SIGSTOP信号给正在前台运作的工作，将其停止
    kill(fgPid, SIGSTOP);
    fgPid = 0;
}

int ctrlC = 0;

void ctrl_C()
{
	//printf("I'm here!");
	if (fgPid == 0)
		return;	
	/*Job *now = head;
	while (now != NULL && now->pid != fgPid)
		now = now->next;
	if (now == NULL)
		printf("added!\n"),now = addJob(fgPid);*/
	ingnore = 0;

	kill(fgPid,SIGKILL);
	//waitpid(fgPid,NULL,0);
	//printf("here!\n");
	fgPid = 0;
	ctrlC = 1;
	//printf(" KILLED!\n");
}

/*fg命令*/
void fg_exec(int pid){    
    Job *now = NULL; 
	int i;
    
    //SIGCHLD信号产生自此函数
    ingnore = 0;
    
	//根据pid查找作业
    now = head;
	while(now != NULL && now->pid != pid)
		now = now->next;
    
    if(now == NULL){ //未找到作业
        printf("pid为7%d 的作业不存在！\n", pid);
        return;
    }

    //记录前台作业的pid，修改对应作业状态
    fgPid = now->pid;
    strcpy(now->state, RUNNING);
    
    signal(SIGTSTP, ctrl_Z); //设置signal信号，为下一次按下组合键Ctrl+Z做准备
    signal(SIGINT, ctrl_C);
    i = strlen(now->cmd) - 1;
    while(i >= 0 && now->cmd[i] != '&')
		i--;
    now->cmd[i] = '\0';
    
    printf("%s\n", now->cmd);
    
    kill(now->pid, SIGCONT); //向对象作业发送SIGCONT信号，使其运行
    goon = 0;
    signal(SIGUSR1,setGoon);
    while (goon == 0);
    goon = 0;
    while (goon1 == 0);
    goon1 = 0;
    //printf("heyhey!\n");
}

/*bg命令*/
void bg_exec(int pid){
    Job *now = NULL;
    
    //SIGCHLD信号产生自此函数
    ingnore = 1;
    
	//根据pid查找作业
	now = head;
    while(now != NULL && now->pid != pid)
		now = now->next;
    
    if(now == NULL){ //未找到作业
        printf("pid为%d 的作业不存在！\n", pid);
        return;
    }
    
    strcpy(now->state, RUNNING); //修改对象作业的状态
    printf("[%d]\t%s\t\t%s\n", now->pid, now->state, now->cmd);
    
    kill(now->pid, SIGCONT); //向对象作业发送SIGCONT信号，使其运行
}

/*******************************************************
                    命令历史记录
********************************************************/
void addHistory(char *cmd){
    if(history.end == -1){ //第一次使用history命令
        history.end = 0;
        strcpy(history.cmds[history.end], cmd);
        return;
	}
    
    history.end = (history.end + 1)%HISTORY_LEN; //end前移一位
    strcpy(history.cmds[history.end], cmd); //将命令拷贝到end指向的数组中
    
    if(history.end == history.start){ //end和start指向同一位置
        history.start = (history.start + 1)%HISTORY_LEN; //start前移一位
    }
}

/*******************************************************
                     初始化环境
********************************************************/
/*通过路径文件获取环境路径*/
void getEnvPath(int len, char *buf){
    int i, j, last = 0, pathIndex = 0, temp;
    char path[40];
    
    for(i = 0, j = 0; i < len; i++){
        if(buf[i] == ':'){ //将以冒号(:)分隔的查找路径分别设置到envPath[]中
            if(path[j-1] != '/'){
                path[j++] = '/';
            }
            path[j] = '\0';
            j = 0;
            
            temp = strlen(path);
            envPath[pathIndex] = (char*)malloc(sizeof(char) * (temp + 1));
            strcpy(envPath[pathIndex], path);
            
            pathIndex++;
        }else{
            path[j++] = buf[i];
        }
    }
    
    envPath[pathIndex] = NULL;
}

/*初始化操作*/
void init(){
    int fd, n, len;
    char c, buf[80];

	//打开查找路径文件ysh.conf
    if((fd = open("ysh.conf", O_RDONLY, 660)) == -1){
        perror("init environment failed\n");
        exit(1);
    }
    
	//初始化history链表
    history.end = -1;
    history.start = 0;
    
    len = 0;
	//将路径文件内容依次读入到buf[]中
    while(read(fd, &c, 1) != 0){ 
        buf[len++] = c;
    }
    buf[len] = '\0';

    //将环境路径存入envPath[]
    getEnvPath(len, buf); 
    //注册信号   
    signal(SIGTSTP, ctrl_Z);
    signal(SIGINT,ctrl_C);
    struct sigaction action;
    action.sa_sigaction = rmJob;
    sigfillset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO;
    sigaction(SIGCHLD, &action, NULL);
}

/*******************************************************
                      命令解析
********************************************************/
SimpleCmd* handleSimpleCmdStr(int begin, int end){
    int i, j, k;
    int fileFinished; //记录命令是否解析完毕
    char c, buff[10][40], inputFile[30], outputFile[30], *temp = NULL;
    SimpleCmd *cmd = (SimpleCmd*)malloc(sizeof(SimpleCmd));
    
	//默认为非后台命令，输入输出重定向为null
    cmd->isBack = 0;
    cmd->input = cmd->output = NULL;
    
    //初始化相应变量
    for(i = begin; i<10; i++){
        buff[i][0] = '\0';
    }
    inputFile[0] = '\0';
    outputFile[0] = '\0';
    
    i = begin;
	//跳过空格等无用信息
    while(i < end && (inputBuff[i] == ' ' || inputBuff[i] == '\t')){
        i++;
    }
    k = 0;
    j = 0;
    fileFinished = 0;
    int flag = 0;
    temp = buff[k]; //以下通过temp指针的移动实现对buff[i]的顺次赋值过程
    while(i < end){
		/*根据命令字符的不同情况进行不同的处理*/
        switch(inputBuff[i]){ 
            case ' ':
            case '\t': //命令名及参数的结束标志
                temp[j] = '\0';
                j = 0;
                if(!fileFinished){
                    k++;
                    temp = buff[k];
                }
                break;

            case '<': //输入重定向标志
                if(j != 0){
		    //此判断为防止命令直接挨着<符号导致判断为同一个参数，如果ls<sth
                    temp[j] = '\0';
                    j = 0;
                    if(!fileFinished){
                        k++;
                        temp = buff[k];
                    }
                }
                temp = inputFile;
                fileFinished = 1;
                i++;
                break;
                
            case '>': //输出重定向标志
                if(j != 0){
                    temp[j] = '\0';
                    j = 0;
                    if(!fileFinished){
                        k++;
                        temp = buff[k];
                    }
                }
                temp = outputFile;
                fileFinished = 1;
                i++;
                break;
                
            case '&': //后台运行标志
                if(j != 0){
                    temp[j] = '\0';
                    j = 0;
                    if(!fileFinished){
                        k++;
                        temp = buff[k];
                    }
                }
                cmd->isBack = 1;
                fileFinished = 1;
                i++;
                break;
                
            default: //默认则读入到temp指定的空间
                temp[j++] = inputBuff[i++];
                continue;
		}
        
		//跳过空格等无用信息
        while(i < end && (inputBuff[i] == ' ' || inputBuff[i] == '\t')){
            i++;
        }
	}
    
    if(inputBuff[end-1] != ' ' && inputBuff[end-1] != '\t' && inputBuff[end-1] != '&'){
        temp[j] = '\0';
        if(!fileFinished){
            k++;
        }
    }
    glob_t gl[k+1];
	//依次为命令名及其各个参数赋值
    int tot = 0;
    for (i  = 0; i < k; i++)
    {
	gl[i].gl_offs = 0;
	//printf("%s %d\n",buff[i],i);
	glob(buff[i],GLOB_TILDE,0,gl+i);
	tot += gl[i].gl_pathc;
	if (gl[i].gl_pathc == 0)
		tot++;
	//printf("%d\n",gl[i].gl_pathc);
    }
    cmd->args = (char**)malloc(sizeof(char*) * (tot + 1));
    cmd->args[tot] = NULL;
    int top = 0;
    for(i = 0; i<k; i++)
    {
        if (gl[i].gl_pathc == 0)
	{
		cmd->args[top++] = (char*) malloc(sizeof(char) * (strlen(buff[i]) + 1));
		strcpy(cmd->args[top - 1],buff[i]);
	}
        else
	{
		for (j = 0; j < gl[i].gl_pathc; j++)
 		{
	        	cmd->args[top++] = (char*)malloc(sizeof(char) * (strlen(gl[i].gl_pathv[j]) + 1));   
       	 		strcpy(cmd->args[top - 1], gl[i].gl_pathv[j]);
		}
    	}
	globfree(gl+i);
    }
	//如果有输入重定向文件，则为命令的输入重定向变量赋值
    if(strlen(inputFile) != 0){
        j = strlen(inputFile);
        cmd->input = (char*)malloc(sizeof(char) * (j + 1));
        strcpy(cmd->input, inputFile);
    }

    //如果有输出重定向文件，则为命令的输出重定向变量赋值
    if(strlen(outputFile) != 0){
        j = strlen(outputFile);
        cmd->output = (char*)malloc(sizeof(char) * (j + 1));   
        strcpy(cmd->output, outputFile);
    }
    #ifdef DEBUG
    printf("****\n");
    printf("isBack: %d\n",cmd->isBack);
    	for(i = 0; cmd->args[i] != NULL; i++){
    		printf("args[%d]: %s\n",i,cmd->args[i]);
	}
    printf("input: %s\n",cmd->input);
    printf("output: %s\n",cmd->output);
    printf("****\n");
    #endif
    return cmd;
}

void returnSignal()
{
	printf("%d\n",kill(getppid(),SIGUSR1));
}

void ending()
{
	exit(0);
}

/*******************************************************
                      命令执行
********************************************************/
/*执行外部命令*/
void execOuterCmd(SimpleCmd *cmd){
    pid_t pid;
    int pipeIn, pipeOut;
    if(exists(cmd->args[0])){ //命令存在

        if((pid = fork()) < 0){
            perror("fork failed");
            return;
        }
        
        if(pid == 0){ //子进程
	    sigset_t new;
		sigaddset(&new,SIGINT);
		//sigprocmask(SIG_BLOCK,&new,NULL);
		sigaddset(&new,SIGTSTP);
		sigprocmask(SIG_BLOCK,&new,NULL);
		struct sigaction sa;
		sa.sa_flags = 0;
		sa.sa_handler = returnSignal;
		sigaction(SIGCONT,&sa,NULL);
            if(cmd->input != NULL){ //存在输入重定向
                if((pipeIn = open(cmd->input, O_RDONLY, S_IREAD|S_IWRITE)) == -1){
                    printf("不能打开文件 %s！\n", cmd->input);
                    return;
                }
                if(dup2(pipeIn, 0) == -1){
                    printf("重定向标准输入错误！\n");
                    return;
                }
            }
            if(cmd->output != NULL){ //存在输出重定向
                if((pipeOut = open(cmd->output, O_WRONLY|O_CREAT|O_TRUNC, S_IREAD|S_IWRITE)) == -1){
                    printf("不能打开文件 %s！\n", cmd->output);
                    return ;
                }
                if(dup2(pipeOut, 1) == -1){
                    printf("重定向标准输出错误！\n");
                    return;
                }
            }
            
            if(cmd->isBack){ //若是后台运行命令，等待父进程增加作业
		printf("child is %d\n",getpid());
		struct sigaction sa;
		sa.sa_flags = 0;
		sa.sa_handler = setGoon;
		sigaction(SIGUSR1,&sa,NULL);
		kill(getppid(),SIGUSR2);//收到信号，setGoon函数将goon置1，以跳出下面的循环
                while(goon == 0); //等待父进程SIGUSR1信号，表示作业已加到链表中
                goon = 0; //置0，为下一命令做准备
                
                printf("[%d]\t%s\t\t%s\n", getpid(), RUNNING, inputBuff);
                kill(getppid(), SIGUSR1);
            }
            
            justArgs(cmd->args[0]);
            if(execv(cmdBuff, cmd->args) < 0){ //执行命令
                printf("execv failed!\n");
                return;
            }
        }
		else{ //父进程
            if(cmd ->isBack){ //后台命令             
                fgPid = 0; //pid置0，为下一命令做准备
                addJob(pid); //增加新的作业
		signal(SIGUSR2,setGoon);
		while (goon == 0);
		goon = 0;
		printf("send to %d\n",pid);
                printf("%d\n",kill(pid, SIGUSR1)); //子进程发信号，表示作业已加入  
                //等待子进程输出
                signal(SIGUSR1, setGoon);
                while(goon == 0);
                goon = 0;
		//waitpid(pid,NULL,WNOHANG);
            }else{ //非后台命令
                fgPid = pid;
		addJob(fgPid);
                //waitpid(pid, NULL, 0);
		while (goon1 == 0);
		goon1 = 0;
            }
		}
    }else{ //命令不存在
        printf("找不到命令 15%s\n", inputBuff);
    }
}

/*执行命令*/
void execSimpleCmd(SimpleCmd *cmd){
    int i, pid;
    char *temp;
    Job *now = NULL;
    
    if(strcmp(cmd->args[0], "exit") == 0) { //exit命令
        exit(0);
    } else if (strcmp(cmd->args[0], "history") == 0) { //history命令
        if(history.end == -1){
            printf("尚未执行任何命令\n");
            return;
        }
        i = history.start;
        do {
            printf("%s\n", history.cmds[i]);
            i = (i + 1)%HISTORY_LEN;
        } while(i != (history.end + 1)%HISTORY_LEN);
    } else if (strcmp(cmd->args[0], "jobs") == 0) { //jobs命令
        if(head == NULL){
            printf("尚无任何作业\n");
        } else {
            printf("index\tpid\tstate\t\tcommand\n");
            for(i = 1, now = head; now != NULL; now = now->next, i++){
                printf("%d\t%d\t%s\t\t%s\n", i, now->pid, now->state, now->cmd);
            }
        }
    } else if (strcmp(cmd->args[0], "cd") == 0) { //cd命令
        temp = cmd->args[1];
        if(temp != NULL){
            if(chdir(temp) < 0){
                printf("cd; %s 错误的文件名或文件夹名！\n", temp);
            }
        }
    } else if (strcmp(cmd->args[0], "fg") == 0) { //fg命令
        temp = cmd->args[1];
        if(temp != NULL && temp[0] == '%'){
            pid = str2Pid(temp, 1, strlen(temp));
            if(pid != -1){
                fg_exec(pid);
            }
        }else{
            printf("fg; 参数不合法，正确格式为：fg %<int>\n");
        }
    } else if (strcmp(cmd->args[0], "bg") == 0) { //bg命令
        temp = cmd->args[1];
        if(temp != NULL && temp[0] == '%'){
            pid = str2Pid(temp, 1, strlen(temp));
            
            if(pid != -1){
                bg_exec(pid);
            }
        }
		else{
            printf("bg; 参数不合法，正确格式为：bg %<int>\n");
        }
    } else{ //外部命令
        execOuterCmd(cmd);
    }
    
    //释放结构体空间
    for(i = 0; cmd->args[i] != NULL; i++){
        free(cmd->args[i]);
	}
        free(cmd->input);
        free(cmd->output);
}

int built_in(char *buff)
{
	int flag1 = strcmp(buff,"cd");
	int flag2 = strcmp(buff,"history");
	int flag3 = strcmp(buff,"jobs");
	int flag4 = strcmp(buff,"fg");
	int flag5 = strcmp(buff,"bg");
	return !(flag1&flag2&flag3&flag4&flag5);
}

void executePipe(SimpleCmd *arr[],int in,int out,int isBg,int num)
{
	SimpleCmd *cmd = *arr;
	pid_t pid,pid1,pid2;
	SimpleCmd *cmd1 = cmd,*cmd2 = *(arr + 1);
	int fd,rd;
	char *p;
	//printf(">>>>>\n");
	if (cmd2 != NULL)
	{

		if (cmd1->input != NULL)
			in = open(cmd1->input,O_RDONLY);
		if (cmd1->output != NULL)
			out = open(cmd1->output,O_WRONLY|O_CREAT|O_TRUNC,S_IREAD|S_IWRITE);
		unlink(FIFO(num));
		mkfifo(FIFO(num),0666);	
		if ((pid1 = fork()) <0)
		{
			perror("fork failed\n");
			return;
		}
		if (pid1 == 0)
		{	
			{fd = open(FIFO(num),O_WRONLY);//printf("hey!\n");
			dup2(fd,STDOUT_FILENO);//printf("hey1!\n");
			dup2(in,STDIN_FILENO);//printf("hey2!\n");
			dup2(out,STDOUT_FILENO);}//printf("hey3!\n");
			if (built_in(cmd1->args[0]) == 0)
			{
				if (!exists(cmd1->args[0]))
				{
					printf("cannot find command\n");
					return;
				}
				if (execvp(cmd1->args[0],cmd1->args) < 0)
				{
					printf("cmd error!\n");
					close(fd);
				}
			}
			else
			{
				execSimpleCmd(cmd1);	
			}
		}
		else
		{
			if ((pid2 = fork()) == 0)
			{
				fd = open(FIFO(num),O_RDONLY);
				if (cmd1->output == NULL)
				{
					in = fd;
				}
				executePipe(arr+1,in,STDOUT_FILENO,isBg,num+1);
				close(fd);
				exit(0);
			}
			else
			{
				addJob(pid1);
				addJob(pid2);
				if (!isBg)
				{
					waitpid(pid1,NULL,0);
					waitpid(pid2,NULL,0);
					//printf("child finished\n");
				}
				
			}
		}
	}
	else
	{
		//printf("here!\n");
		if (cmd1->input != NULL)
			in = open(cmd1->input,O_RDONLY);
		if (cmd1->output != NULL)
			out = open(cmd1->output,O_WRONLY|O_CREAT|O_TRUNC,S_IREAD|S_IWRITE);
		if ((pid = fork()) == 0)
		{
			{dup2(in,STDIN_FILENO);
			dup2(out,STDOUT_FILENO);}
			if (execvp(cmd1->args[0],cmd1->args) < 0)
			{
				printf("error\n");
				exit(-1);
			}
			exit(0);
		}
		else
		{
			addJob(pid);
			if (!isBg)
			{
				//printf("zusenimabi\n");
				waitpid(pid,NULL,0);
				//printf("oh!\n");
			}
		}	
	}

	if (in != STDIN_FILENO) close(in);
	if (out != STDOUT_FILENO) close(out);
	int i;
	for (i = 0; cmd->args[i]!= NULL;i++)
	{
		free(cmd->args[i]);}
	free(cmd->input),free(cmd->output);
				//printf("%d\n",1981);
	return;
}


/*******************************************************
                     命令执行接口
********************************************************/
void execute(){
	char *p;
	printf("%s\n",inputBuff);
	if ((p = strchr(inputBuff,'|')) == NULL)
	{
		SimpleCmd *cmd = handleSimpleCmdStr(0, strlen(inputBuff));

		execSimpleCmd(cmd);
	}
	else//pipe
	{
		char *ptr = inputBuff;	
		SimpleCmd *cmd[20];//malloc(sizeof(SimpleCmd*) * 20);
		int tot = 0;
		int begin = 0;
		do
		{
			cmd[tot++] = handleSimpleCmdStr(begin,(p - 1) - (inputBuff)); 
			begin = p - inputBuff;
			begin++;
		}
		while ((p = strchr(inputBuff + begin,'|')) != NULL);
		cmd[tot++] = handleSimpleCmdStr(begin,strlen(inputBuff));
		cmd[tot++] = NULL;
		pid_t pid;
		if ((pid = fork()) == 0)
		{
			executePipe(cmd,STDIN_FILENO,STDOUT_FILENO,cmd[tot - 2]->isBack,0);
		//printf("%d\n",2015);
		}
		{
			addJob(pid);
			while (goon1 == 0);
			goon1 = 0;
		}
			
	}								
}
