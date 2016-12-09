/*************************************************************************
    > File Name: myshell.c
    > Author: Bin Xu
    > Mail: x_shares@outlook.com
    > Created Time: 2016年12月08日 星期四 18时46分01秒
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/utsname.h>

#define		MAX_BUFF		1024		//用户输入的最大长度
#define		MAX_COMM		128			//单个命令及其参数最大字符长度

#define		INCREASE		0		//链表插入
#define		DELETE			1		//链表节点删除
#define		PRINT			2		//打印链表

char cwd[MAX_BUFF];			//当前路径
char *commands;
char *all_command[MAX_COMM];	//折分后的单条命令及其参数
int		current_out;
int		current_in;

//后台进程结构体
struct proc {
	pid_t	pid;
	int		status;
	char	*argv[MAX_COMM];
	struct proc		*next;
};

typedef struct proc		proc;
proc *head = NULL;		//头结点

void print_prompt(void)
{
	struct utsname		hostinfo;
	uname(&hostinfo);
	getcwd(cwd,sizeof(cwd));

	setbuf(stdout,NULL);
	printf("<%s@%s:%s $ ",hostinfo.nodename,hostinfo.sysname,cwd);
}

void sig_handler(int sig)
{
	if(sig == SIGINT) {
		printf("\nInstead of Ctrl-c type quit\n");
		print_prompt();
	}
	else if(sig == SIGQUIT) {
		printf("\nType quit to exit\n");
		print_prompt();
	}

	signal(sig,sig_handler);
}

void scan_user_input(void)
{
	ssize_t nbyte;
	size_t len = MAX_BUFF;

	nbyte = getline(&commands,&len,stdin);
	commands[nbyte - 1] = '\0';
}

void parse_by_semicolon(char *command)
{
	int count = 0;
	
	count = 0;
	all_command[count] = strtok(command,";");

	while(1) {
		count ++;
		all_command[count] = strtok(NULL,";");
		if(all_command[count] == NULL)
			break;
	}
}

void clean(void)
{
	proc *iter = head;
	proc *next;
	int count;
	for(count = 0;all_command[count] != NULL;count++)
		free(all_command[count]);
	while(iter != NULL) {
		next = head -> next;
		for(count = 0;iter->argv[count] != NULL;count ++)
			free(iter->argv[count]);
		free(iter);
		iter = next;
	}
}

#if 0
void print_argement(char **all_command)
{
	int count;
	for(count = 0;all_command[count] != NULL;count++) {
		printf("%s\n",all_command[count]);
	}
}
#endif

void bg_struct_handler(pid_t pid,char **argv,int type)
{
	int count;
	proc *new,*iter;
	if(type == INCREASE) {
		new = (proc *)malloc(sizeof(proc));
		new->pid = pid;
		new->status = 1;
		new->next = NULL;
		for(count = 0;argv[count] != NULL;count ++) {
			new->argv[count] = (char *)malloc(MAX_COMM);
			strcpy(new->argv[count],argv[count]);
		}
		new->argv[count] = NULL;
		if(head == NULL) {
			head = new;
		}
		else {
			iter = head;
			while(iter->next != NULL)
				iter = iter->next;
			iter->next = new;
		}
	}
	else if(type == DELETE) {
		proc *preiter = NULL;
		iter = head;
		while(iter != NULL && iter->pid != pid) {
			preiter = iter;
			iter = iter->next;
		}
		if(iter == NULL) {
			printf("No Such Pid !\n");
			return ;
		}
		else if(iter->pid == pid) {
			if(preiter == NULL) 
				head = iter->next;
			else
				preiter->next = iter->next;

			for(count = 0;iter->argv[count] != NULL;count++) {
				free(iter->argv[count]);
			}
			free(iter);
		}
	}
	else if(type == PRINT) {
		count = 0;
		iter = head;
		if(iter == NULL) {
			printf("No Backgroud Jobs\n");
			return ;
		}
		while(iter != NULL) {
			setbuf(stdout,NULL);
			printf("[%d]",count+1);
			while(iter->argv[count] != NULL) {
				printf("%s ",iter->argv[count]);
				count++;
			}
			printf("[%d]\n",iter->pid);
			iter = iter->next;
		}
	}
	return ;
}

void bg_signal_handler(int sig)
{
	int status;
	pid_t pid;
	proc *iter = head;
	pid = waitpid(-1,&status,WNOHANG);
	while(iter != NULL) {
		if(iter->pid == getpid())
			bg_struct_handler(pid,NULL,1);
	}
}

void bf_exec(char **argv,int type)
{
	pid_t pid;
	int status;
	if(type == 0) {				//前台进程
		if((pid = fork()) < 0) {
			printf("*** ERROR:froking child process faild\n");
			return ;
		}
		else if(pid == 0) {
			signal(SIGTSTP,SIG_DFL);
			execvp(argv[0],argv);
		}
		else {
			 pid_t c;
			 signal(SIGTSTP,SIG_DFL);
			 c = wait(&status);
			 dup2(current_out,1);
			 dup2(current_in,0);
			 return ;
		}
	}
	else {						//后台进程
		signal(SIGCHLD,bg_signal_handler);
		if((pid = fork()) < 0) {
			printf("*** ERROR:forking child process faild\n");
			return ;
		}
		else if(pid == 0) {
			execvp(argv[0],argv);
		}
		else {
			bg_struct_handler(pid,argv,0);
			dup2(current_out,1);
			dup2(current_in,0);
			return ;
		}
	}
}

void file_out(char **argv,char *out_file,int type)
{
	int		fd_out;
	if(type == 0)	//处理>情况
		fd_out = open(out_file,O_WRONLY | O_CREAT,0777);
	else			//处理>>情况
		fd_out = open(out_file,O_WRONLY | O_CREAT | O_APPEND,0777);
	dup2(fd_out,1);
	close(fd_out);
	bf_exec(argv,0);
}

//处理重定向的输入问题,其中包裹了重定向输出
void file_in(char **argv,char *in_file,char *out_file,int type)
{
	int fd_in;
	fd_in = open(in_file,O_RDONLY);
	dup2(fd_in,0);
	close(fd_in);
	if(type == 0) {			//没有重定向输出
		printf("Going to execute bf_exec\n");
		bf_exec(argv,0);
	}
	else if(type == 1)		//重定向输出 >
		file_out(argv,out_file,0);
	else					//重定向输出 >>
		file_out(argv,out_file,1);
}

void execute(char *single_command)
{
	//分解每条命令的命令及参数
	char *argv[MAX_COMM];		//命令及其参数存放位置
	char *single;
	int argv_count = 1;
	char *out_file_flag;
	char *out_file;

	argv[0] = strtok(single_command," ");
	argv[1] = NULL;

	if(strcmp(argv[0],"cd") == 0) {
		single = strtok(NULL," ");
		chdir(single);
	}
	if(strcmp(argv[0],"exit") == 0) {
		clean();
		printf("clean over\nprocess will exit\n");
		exit(0);
	}

//处理输入输出重定向,前台后台进程
	while(1) {
		single = strtok(NULL," ");
		if(single == NULL)				//没有重定向
			break;
		else if(strcmp(single,">") == 0) { //输出重定向
			single = strtok(NULL," ");
			file_out(argv,single,0);
			return ;
		}
		else if(strcmp(single,">>") == 0) {	//输出重定向
			single = strtok(NULL," ");
			file_out(argv,single,1);
			return ;
		}
		else if(strcmp(single,"<") == 0) { //输入重定向
			single = strtok(NULL," ");	//输入重定向文件名
			out_file_flag = strtok(NULL," ");
			if(strcmp(out_file_flag,">") == 0) {
				out_file = strtok(NULL," ");
				if(out_file == NULL) {
					printf("Syntax error\n");
					return ;
				}
				else
					file_in(argv,single,out_file,1);
			}
			else if(strcmp(out_file_flag,">>") == 0) {
				out_file = strtok(NULL," ");
				if(out_file == NULL) {
					printf("Syntax error\n");
					return ;
				}
				else
					file_in(argv,single,out_file,2);
			}
			else
				file_in(argv,single,NULL,0);
		}
		else if(strcmp(single,"&") == 0) {
			bf_exec(argv,1);
			return ;
		}
		else {
			argv[argv_count] = single;
			argv_count ++;
			argv[argv_count] = NULL;
		}
	}

	bf_exec(argv,0);
	return ;
}

int main(int argc,char **argv)
{
	int count;
	int commands_num;
	current_out = dup(1);
	current_in = dup(0);
	commands = (char *)malloc(sizeof(MAX_BUFF));

	for(;count < MAX_COMM;count ++) {
		all_command[count] = (char *)malloc(MAX_COMM);
	}

	for( ; ; ) {
		commands_num = 0;
		commands = NULL;
		for(count = 0;count < MAX_COMM;count ++)
			all_command[count] = NULL;
		//信号处理
		signal(SIGINT,sig_handler);
		signal(SIGQUIT,sig_handler);
		signal(SIGCHLD,sig_handler);
		signal(SIGTSTP,SIG_IGN);

		print_prompt();				//打印提示符
		scan_user_input();	//读入用户输入
		parse_by_semicolon(commands);//以分号拆分命令
//		print_argement(all_command);
#if 1
		//依次执行每条命令
		while(all_command[commands_num] != NULL) {
			execute(all_command[commands_num]);
			commands_num ++;
		}
#endif
	}
}
