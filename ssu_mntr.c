#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>

#define BUFLEN 1024 //버퍼의 길이
#define SECOND_TO_MICRO 1000000 //초를 마이크로초로 변경할때 사용

struct info{//파일에 대한 정보를 담는 구조체
	char name[BUFLEN];
	time_t m_time;
};

typedef struct info file_info;

void ssu_runtime(struct timeval *begin_t,struct timeval *end_t);//프로그램 수행시간 측정 함수
void print_usage(void);//입력형식 호출해주는 함수
void showprompt();//프롬프트를 출력하는 함수
void delete_file(char*,int,int);//파일을 삭제하는 함수("delete" 수행)
void recover_file(char*,int);//파일을 복구하는 함수("recover" 수행)
void show_tree(char*,int);//트리를 보이는 함수("tree" 수행)
int check_dir_size();//디렉터리 사이즈를 측정하는 함수
void delete_old_file();//제일 오래된 파일 지우는 함수
void delete_files_in_dir(char*);//디렉터리 안에 있는 파일 지우는 함수
void delete_empty_dir(char*);//빈 디렉터리 지우는 함수
int daemon_init(void);//디몬 프로세스를 초기화하는 함수
void execute_daemon(char*);//디몬 프로세스를 실행하는 함수
int file_scandir(char*,struct info*,int*);//파일을 스캔하여서 info구조체에 정보를 담는 함수
void checkfile(struct info *,struct info *,int,int);//파일의 삭제, 추가, 수정을 확인하는 함수
char* real_path(char*,char*);//절대경로로 변환해주는 함수
int ret_sec(int,int,int,int,int);//"delete" 명령어에서 할당해준 시간을 초로 변화하는 함수
int is_samename(DIR*,char*);//동일한 이름파일이 있는지 찾는함수
char checkpath[BUFLEN];//특정 디렉토리 절대경로저장
char trashpath[BUFLEN];//trash 디렉토리 절대경로 저장
char filespath[BUFLEN];//trash/files디렉토리 절대경로 저장
char infopath[BUFLEN];//trash/info 디렉토리 절대경로 저장
char savedpath[BUFLEN];//모든 디렉토리를 담는 디렉토리 경로 저장
int indexnum=0;//파일의 전체개수
struct stat statbuf;//stat구조체버퍼
struct tm* t;//tm구조체 정보

int main(int argc, char *argv[])
{
	struct timeval begin_t,end_t;
	gettimeofday(&begin_t,NULL);
	FILE *fp;
	int daempid;
	//각종 경로 초기화
	memset(checkpath,0,BUFLEN);
	memset(trashpath,0,BUFLEN);
	memset(filespath,0,BUFLEN);
	memset(infopath,0,BUFLEN);
	memset(savedpath,0,BUFLEN);

	//프로그램실행하는 디렉토리 절대경로 저장
	getcwd(savedpath,BUFLEN);

	//check디렉토리생성
	if(access("check",F_OK)<0)
		mkdir("check",0755);
	
	chdir("check");
	getcwd(checkpath,BUFLEN);//디렉토리 경로 저장

	chdir(savedpath);
	//log.txt 생성 및 이어붙이기
	fp=fopen("log.txt","a+");

	//trash디렉토리생성
	if(access("trash",F_OK)<0)
		mkdir("trash",0755);

	chdir("trash");
	getcwd(trashpath,BUFLEN);

	// trash/files 디렉토리 생성
	if(access("files",F_OK)<0)
		mkdir("files",0755);

	chdir("files");
	getcwd(filespath,BUFLEN);//디렉토리 경로 저장

	chdir(trashpath);

	// trash/info 디렉토리 생성
	if(access("info",F_OK)<0)//info디렉토리 생성
		mkdir("info",0755);

	chdir("info");
	getcwd(infopath,BUFLEN);//디렉토리 경로 저장

	//디몬프로세스 실행
    if((daempid=fork())<0){
		fprintf(stderr,"fork error\n");
	//	exit(0);
	}
	else if(daempid==0){
		if(daemon_init()<0){
			fprintf(stderr,"daemon initialization error\n");
	//		exit(0);
		}
	}
	chdir(savedpath);
	
	//프롬프트 실행
	showprompt();

	//프로그램 수행 시간측정
	gettimeofday(&end_t,NULL);
	ssu_runtime(&begin_t,&end_t);
	exit(0);
}
/*프로그램 수행시간 측정 함수*/
void ssu_runtime(struct timeval *begin_t, struct timeval *end_t)
{
	end_t->tv_sec -= begin_t->tv_sec;
	
	if(end_t -> tv_usec < begin_t -> tv_usec){
		end_t->tv_sec--;
		end_t->tv_usec += SECOND_TO_MICRO;
	}

	end_t->tv_usec-=begin_t->tv_usec;
	printf("Runtime: %ld:%06ld(sec:usec)\n",end_t->tv_sec,end_t->tv_usec);
}
/*프롬프트 출력함수*/
void showprompt(char *checkDir)
{
	int i=0;
	int sec=0;//초 저장
	char command[BUFLEN];
	char *cArr[BUFLEN];//띄어쓰기를 기준으로 문자열 저장되는 이차원 배열
	char mode[BUFLEN];//delete, size,recover,tree,exit,help 명령어 저장
	char filename[BUFLEN];//파일이름 저장

	while(1){
		i=0;
		int doption=-1;//delete 일때 옵션
		memset(command,0,BUFLEN);//command 초기화
		printf("20182620>");//학번출력하며 프롬프트 출력
		fgets(command,BUFLEN,stdin);
		command[strlen(command)-1]=0;//개행문자 없앰 
		if(!strncmp(command,"\0",1)){//학번 출력후 띄어쓰기가 끝날때까지 반복
			continue;
		}
		char *ptr=strtok(command," ");
		while(ptr!=NULL)//문자열끝까지 띄어쓰기를 기준으로 배열에 저장
		{
			cArr[i]=ptr;
			i++;
			ptr=strtok(NULL," ");
		}
		
		strcpy(mode,cArr[0]);
		if((strcmp(mode,"delete"))&&(strcmp(mode,"size"))&&(strcmp(mode,"recover"))&&(strcmp(mode,"tree"))&&(strcmp(mode,"exit"))&&(strcmp(mode,"help")))
		{
			//help와 동일한 표준출력
			print_usage();
			continue;
		}//이외의 명령어 수행시 자동으로 help를 실행시킨 것과 동일하게 표준출력 후 프롬프트 출력
		if(!strcmp(mode,"delete")){//delete옵션일 경우
			int year,month,day,hour,minute;
			if(cArr[1]==NULL){//파일이름 입력없을때
				fprintf(stderr,"please enter delete filename\n");
				continue;
			}
			if(!strcmp(cArr[1],"\0")){//파일명 공간이 공백일때
				fprintf(stderr,"please enter delete filename\n");
				continue;
			}
			if(cArr[2]!=NULL){
				//들어온 값이 시간인지 옵션인지 검사
				//옵션일경우
				if(!strcmp(cArr[2],"-i"))
					doption=0;
				else if(!strcmp(cArr[2],"-r"))
					doption=1;

				//시간일경우
				else
				{
					if(sscanf(cArr[2],"%d-%d-%d",&year,&month,&day)==0){
						fprintf(stderr,"input correct delete date\n");
						continue;
					}
					if(sscanf(cArr[3],"%d:%d",&hour,&minute)==0){
						fprintf(stderr,"input correct delete time\n");
						continue;
					}
					//초를 반환하는 함수. 과거 시간 입력시 에러처리
					if((sec=ret_sec(year,month,day,hour,minute))==-1){
						fprintf(stderr,"input future time\n");
						continue;
					}
				}
			}
			//"delete" 의 옵션을 입력했을 때
			if(cArr[4]!=NULL){
				if(!strcmp(cArr[2],"-i"))
					doption=0;
				else if(!strcmp(cArr[2],"-r"))
					doption=1;
				else
				{
					fprintf(stderr,"input correct delete format data\n");
					continue;
				}
			}
			delete_file(cArr[1],sec,doption);
		}
		//"size" 입력했을때
		if(!strcmp(mode,"size")){
			
		}
		//"recover" 입력했을때
		if(!strcmp(mode,"recover")){
			int roption=-1;
			//파일이름 입력안했을때
			if(cArr[1]==NULL){
				fprintf(stderr,"please enter recover filename\n");
				continue;
			}
			if(!strcmp(cArr[1],"\0")){
				fprintf(stderr,"please enter recover filename\n");
				continue;
			}
			//"recover" 옵션 입력했을때
			if(cArr[2]!=NULL){
				if(!strcmp(cArr[2],"-l"))
					roption=0;
				else 
				{
					fprintf(stderr,"please enter correct -l option\n");
					continue;
				}
			}
			//복구함수실행
			recover_file(cArr[1],roption);
		}
		//"tree"를 입력했을때
		if(!strcmp(mode,"tree")){
			printf("check");
			show_tree(checkpath,1);//트리출력함수 실행
			continue;
		}
		if(!strcmp(mode,"exit")){//프로그램 종료시키는 명령어
			printf("program exit!!\n");
			break;
		}
		if(!strcmp(mode,"help")){//명령어 사용법 출력하는 명령어
			print_usage();
			continue;
		}
	}
}
int ret_sec(int year,int month,int day,int hour,int minute)
{
	time_t t;
	struct tm t_in; 
	struct tm *t_now;
	int input_s;//입력받은 시간 초변환
	int now_s;//현재시간 초변환
	int sec;

	/*입력받은 시간을 tm구조체에 입력*/
	t_in.tm_year=year-1900;
	t_in.tm_mon=month-1;
	t_in.tm_mday=day;
	t_in.tm_hour=hour;
	t_in.tm_min=minute;
	t_in.tm_sec=0;

	mktime(&t_in);

	/*현재시간 받기*/
	t=time(NULL);
	t_now=localtime(&t);
	mktime(t_now);

	/*각각 초로 변환*/
	input_s=((t_in.tm_yday)*1440+(t_in.tm_hour)*60+(t_in.tm_min))*60;
	now_s=((t_now->tm_yday)*1440+(t_now->tm_hour)*60+(t_now->tm_min))*60;
	
	if((sec=input_s-now_s)<0)//inputtime이 과거일경우
		return -1;

	sec=input_s - now_s;
	return sec;
}
	
void delete_file(char *filename,int sec,int doption)
{//파일을 삭제하는 함수
	int count=0;
	char* tmp;//filename에 대한 절대경로
	char file[BUFLEN];//filename에 대한 파일이름
	char copyfile[BUFLEN];//중복될경우 숫자_을 추가한 파일이름
	char* pch;
	FILE* fp;
	DIR *dp;
	DIR *cdp;//checkDir 포인터
	time_t delete_t;
	time_t modify_t;
	struct tm* time_m;
	struct tm* time_d;
	struct dirent *dentry;//checkDir 정보저장 구조체
	char str[BUFLEN];
	
	chdir(checkpath);
	/*filename에 대한 절대/상대경로 변환*/
	tmp=(char*)malloc(sizeof(char)*BUFLEN);

	tmp=real_path(filename,tmp);//filename을 절대경로로 변환

	pch=strrchr(tmp,'/');
	memset(file,0,BUFLEN);
	strcpy(file,pch+1);//filename의 파일이름

	if(stat(tmp,&statbuf)<0){
		fprintf(stderr,"stat error for delete file\n");
		return;
	}
	
	modify_t=statbuf.st_mtime;
	time_m=localtime(&modify_t);	
	sprintf(str,"%d-%d-%d %d:%d:%d\n",1900+time_m->tm_year,time_m->tm_mon+1,time_m->tm_mday,time_m->tm_hour,time_m->tm_min,time_m->tm_sec);//info파일에 쓸 수정시간정보 저장

	pid_t pid;
	if((pid=fork())<0){
		fprintf(stderr,"fork error\n");
		exit(1);
	}
	else if(pid>0){//부모의 경우
		return;
	}
	else//자식의 경우
	{
		sleep(sec);//설정한 시간에 삭제

		/*삭제될 파일이 중복되는지 확인*/
		chdir(filespath);
		char buf[BUFLEN];
		memset(buf,0,BUFLEN);
		sprintf(buf,"%s/%s",filespath,file);//filesDir에 동일한 파일이름이 있는지 확인
		while(access(buf,F_OK)==0)//중복된 파일 존재안할때까지 반복
		{
			count++;
			sprintf(buf,"%s/%d_%s",filespath,count,file);
		}	
		if(count==0)
			sprintf(buf,"%s/%s",filespath,file);

		//마지막으로 변경된 buf가 최종 files의 파일명이됨
		rename(tmp,buf);//trash/files로 삭제파일이동

		//파일의 삭제시간 측정
		time(&delete_t);
		time_d=localtime(&delete_t);

		/*info디렉터리에 삭제파일 정보입력*/
		chdir(infopath);
		char buf2[BUFLEN];
		memset(buf2,0,BUFLEN);
		if(count==0)//중복된 info파일이 없을때
			sprintf(buf2,"%s",file);
		else//중복파일이 있을때
			sprintf(buf2,"%d_%s",count,file);
	
		if((fp=fopen(buf2,"w+"))<0){//infoDir에 삭제파일 정보가 담긴 파일 만들기
			fprintf(stderr,"2.fopen error in infoDir\n");
			return;
		}
		fprintf(fp,"[Trash info]\n");
		fprintf(fp,"%s\n",tmp);
		fprintf(fp,"D : %d-%d-%d %d:%d:%d\n",1900+time_d->tm_year,time_d->tm_mon+1,time_d->tm_mday,time_d->tm_hour,time_d->tm_min,time_d->tm_sec);//파일삭제시간
		fprintf(fp,"M : %s",str);//파일최종수정시간
		fclose(fp);

		//infoDir가 2KB초과할경우, 오래된파일부터 지움
		long int totalsize=0;
		totalsize=check_dir_size();
		if(totalsize>2000)
		{	while(totalsize>2000)
			{
				delete_old_file();//가장 오래된 함수 지우는 함수
				totalsize=check_dir_size();//info파일크기 측정. 2KB이하가 될때까지 반복
			}	
		}
		exit(0);//자식프로세스 종료
	}
	return;
}
/*info 디렉토리 사이즈 확인하는 함수*/
int check_dir_size()
{
	int num;
	int i;
	int count=0;
	struct dirent **files;
	struct stat fstatbuf;
	time_t newtime;
	time_t oldtime;
	time_t temptime=0;
	int totalsize=0;
	chdir(infopath);

	num=scandir(".",&files,NULL,alphasort);//디렉토리 내부 파일개수 리턴

	for(i=0;i<num;i++)//파일개수만큼 반복
	{
		if((!strcmp(files[i]->d_name,"."))||(!strcmp(files[i]->d_name,"..")))
			continue;
		count++;
		lstat(files[i]->d_name,&fstatbuf);
		totalsize+=fstatbuf.st_size;//파일사이즈 측정
	}
	chdir("..");//다시 자신 info 디렉토리로 이동
	return totalsize;//총 파일사이즈 리턴
}
/*가장 오래된 파일을 지우는 함수*/
void delete_old_file()
{
	struct dirent **items;
	int num;
	int d_num;
	int i;
	int old_count=0;
	time_t oldtime;
	time_t newtime;
	char oldfile[BUFLEN];
	char old_infopath[BUFLEN];
	char old_filespath[BUFLEN];
	struct stat fstatbuf;

	chdir(infopath);//info디렉토리로 이동

	num=scandir(".",&items,NULL,alphasort);//파일개수

	for(i=0;i<num;i++)//파일개수만큼 반복
	{

		if((!strcmp(items[i]->d_name,"."))||(!strcmp(items[i]->d_name,"..")))
			continue;

		lstat(items[i]->d_name,&fstatbuf);//stat구조체 설정
		old_count++;
		if(old_count==1)//처음 들어온 파일은 먼저 oldtime에 자신의 수정시간을 넣는다
		{
			oldtime=fstatbuf.st_mtime;
			d_num=i;//삭제될 파일인덱스. 이것을 통해 무엇이 삭제될지를 안다
		}
		else
		{
			if(oldtime>fstatbuf.st_mtime)//새로 들어온 시간이 더 예전일때
			{
				oldtime=fstatbuf.st_mtime;//oldtime에 할당
				d_num=i;//가장 오래된 파일이 바뀜
			}
		}
	}
	//info,files에서 해당하는 파일 제거
	strcpy(oldfile,items[d_num]->d_name);
	chdir(infopath);
	remove(oldfile);
	chdir(filespath);
	remove(oldfile);
	//"old_filespath"가 디렉토리일 경우
	//만약 빈 디렉토리가 아니라면 remove 할수없다.
	sprintf(old_filespath,"%s/%s",filespath,oldfile);
	stat(old_filespath,&fstatbuf);
	if(S_ISDIR(fstatbuf.st_mode))//이 파일이 빈 디렉토리가 아닐 경우
	{
		delete_files_in_dir(old_filespath);//디렉토리안에 파일 삭제
		delete_empty_dir(old_filespath);//빈디렉토리들을 순차적으로 삭제
	}
	chdir("..");//다시 info로 돌아감
	return;
}
void delete_files_in_dir(char *dir)//디렉토리 안의 파일 삭제
{
	struct dirent **items;
	struct stat fstat;
	int num;
	int i;
	char old_filespath2[BUFLEN];

	chdir(dir);
	num=scandir(".",&items,NULL,alphasort);//해당 디렉토리의 파일 개수

	for(i=0;i<num;i++)
	{
		if((!strcmp(items[i]->d_name,"."))||(!strcmp(items[i]->d_name,"..")))
			continue;

		lstat(items[i]->d_name,&fstat);
		remove(items[i]->d_name);//파일 삭제. 빈 디렉토리가 아닐경우 삭제 되지 않음
		if(S_ISDIR(fstat.st_mode))//디렉토리일 경우
		{
			delete_files_in_dir(items[i]->d_name);//다시 재귀
		}
	}
	chdir("..");
	return;
}
void delete_empty_dir(char *dir)//빈 디렉토리 삭제
{
	struct dirent **items;
	struct stat fstat;
	int num;
	int i;
	char old_dirpath[BUFLEN];

	chdir(dir);
	num=scandir(".",&items,NULL,alphasort);

	for(i=0;i<num;i++)
	{
		if((!strcmp(items[i]->d_name,"."))||(!strcmp(items[i]->d_name,"..")))
			continue;

		lstat(items[i]->d_name,&fstat);
		remove(items[i]->d_name);//빈 디렉토리 삭제
		if(S_ISDIR(fstat.st_mode))//디렉토리일 경우
			delete_empty_dir(items[i]->d_name);//재귀
	}
	chdir("..");
	return;
}
/*recover 수행함수*/
void recover_file(char *filename,int roption)
{
	DIR *dp;
	struct dirent **items;
	int n;
	char copyfile[BUFLEN];//파일이름 복사하는 문자열
	char buf[BUFLEN];
	char file[BUFLEN];
	char *tmp;
	char *pch;
	FILE *fp;
	char r_path[BUFLEN];//checkDir에 복구하는 파일경로
	char f_path[BUFLEN];//filesDir의 해당파일경로
	char i_path[BUFLEN];//infoDir의 해당파일경로
	memset(r_path,0,BUFLEN);
	memset(f_path,0,BUFLEN);
	memset(i_path,0,BUFLEN);
	memset(copyfile,0,BUFLEN);
	
	char* lch;
	int count=0;
	int i;

	n=scandir(infopath,&items,NULL,alphasort);//info디렉토리의 파일개수
	int samelist[n];

	chdir(infopath);//info디렉토리로 이동
	for(i=0;i<n;i++)
	{
		memset(buf,0,BUFLEN);
		if(!strcmp(items[i]->d_name,".")||!strcmp(items[i]->d_name,".."))
			continue;

		if((fp=fopen(items[i]->d_name,"r"))==NULL){//trash/info안에 있는 파일열기
			fprintf(stderr,"fopen error for %s\n",items[i]->d_name);
			return;
		}
		if(fseek(fp,strlen("[Trash info]\n"),SEEK_SET)!=0){
			fprintf(stderr,"fseek error for %s\n",items[i]->d_name);
			return;
		}
		if(fgets(buf,sizeof(buf),fp)!=NULL)//info 해당파일에서 두번째 줄 읽음
		{//맨뒤 '/'뒤의 파일이름 받음
			buf[strlen(buf)-1]='\0';
			lch=strrchr(buf,'/');
			if(!strcmp(lch+1,filename)){//같은파일이름의 복구파일찾기. 
				count++;//중복될경우
				samelist[count]=i;
			}
		}
		fclose(fp);
	}
	/*check에 동일한 이름의 파일 찾기*/
	int number=0;
	memset(buf,0,BUFLEN);
	sprintf(buf,"%s/%s",checkpath,filename);//filesDir에 동일한 파일이름이 있는지 확인
	while(access(buf,F_OK)==0)//중복된 파일 존재안할때까지 반복
	{
		number++;
		sprintf(buf,"%s/%d_%s",checkpath,number,filename);
	}	
	if(number==0)
		sprintf(buf,"%s/%s",checkpath,filename);

	/*info에 같은 파일 이름이 있을경우. 프롬프트에서 삭제할 파일 선택*/
	char d_arr[BUFLEN];//deletetime저장
	char m_arr[BUFLEN];//modifytime저장
	char samename[BUFLEN];//같은 이름의 파일 저장
	char buf1[BUFLEN];
	char buf2[BUFLEN];
	char buf3[BUFLEN];
	char buf4[BUFLEN];
	int k=1;
	if(count>1)//info에 동일한 파일이 여러개 있을경우.프롬프트를 통해 선택받음
	{
		for(i=1;i<=count;i++)
		{
			strcpy(samename,items[samelist[i]]->d_name);
			if((fp=fopen(samename,"r"))==NULL){//trash/info안에 있는 파일열기
				fprintf(stderr,"fopen error for %s\n",samename);
				return;
			}
			//파일의 info정보를 한줄씩 입력받음
			fgets(buf1,sizeof(buf1),fp);
			fgets(buf2,sizeof(buf2),fp);
			fgets(buf3,sizeof(buf3),fp);
			fgets(buf4,sizeof(buf4),fp);
			buf3[strlen(buf3)-1]='\0';
			buf4[strlen(buf4)-1]='\0';
			printf("%d. %s  %s  %s\n",i,samename,buf3,buf4);//파일의 정보
		}
		printf("Choose : ");
		scanf("%d",&k);//선택
		getchar();
	}
	else if(count==1)//복구파일이 하나일경우
		k=1;
	else
	{
		fprintf(stderr,"There is no '%s' in the 'trash' directory\n",filename);
		return;
	}
		
	/*check에서  동일한 이름의 파일 찾기*/
		int j=0;
		number=0;
		char realname[BUFLEN];
		memset(realname,0,BUFLEN);
		if((fp=fopen(items[samelist[k]]->d_name,"r"))==NULL){//복구될 파일의 info열기
			fprintf(stderr,"fopen error for %s\n",items[samelist[k]]->d_name);
			return;
		}
		fgets(buf1,sizeof(buf1),fp);
		fgets(buf2,sizeof(buf2),fp);
		buf2[strlen(buf2)-1]='\0';//파일의 복구주소 읽기
		int total;
		int n1;
		int n2;
		char buffer[BUFLEN];
		while(access(buf2,F_OK)==0)//중복된 파일 존재안할때까지 반복
		{
			number++;
			lch=strrchr(buf2,'/');//맨끝에 단어 읽기. 파일명
			n1=strlen(buf2)-1;
			n2=strlen(lch)-1;
			total=n1-n2;
			for(j=0;j<total;j++)//파일명을 뺀 나머지 경로 받기
				buffer[j]=buf2[j];
			sprintf(realname,"%d_%s",number,filename);//중복된다면 숫자_붙이기
			sprintf(buf2,"%s/%s",buffer,realname);//실제 복구 경로
			strcpy(r_path,buf2);
		}
		if(number==0)//중복파일 존재안할때
			strcpy(r_path,buf2);

		//filesDir에서 checkDir로 rename
		sprintf(f_path,"%s/%s",filespath,items[samelist[k]]->d_name);
		if(rename(f_path,r_path)==-1)
		{
			fprintf(stderr," error for %s\n",items[samelist[k]]->d_name);
			return;
		}
		sprintf(i_path,"%s/%s",infopath,items[samelist[k]]->d_name);
		remove(i_path);//info 파일 제거

	fclose(fp);
}

/*절대경로로 변경해주는 함수*/
char* real_path(char *filename,char *tmp)
{
	if(filename[0]=='.')//상대경로, 파일이름인 경우
	{
		if(realpath(filename,tmp)==NULL)
		{
			fprintf(stderr,"this file doesn't exist.\n");
			exit(1);
		}
		return tmp;
	}
	else if(filename[0]=='/')//절대경로인 경우 변환할 필요 없음
		return filename;
	
	else
	{
		if(realpath(filename,tmp)==NULL)
		{//경로가 없을 경우
			fprintf(stderr,"this file doesn't exist.\n");
			exit(1);
		}
		return tmp;
	}
}
void show_tree(char *dir,int level)
{//트리 출력
	struct dirent **tree;
	int num;
	int i;
	int count=0;

	chdir(dir);//해당디렉토리변경

	num=scandir(".",&tree,NULL,alphasort);
	count =0;
	for(i=0;i<num;i++)
	{
		struct stat tstat;

		if((!strcmp(tree[i]->d_name,"."))||(!strcmp(tree[i]->d_name,"..")))
			continue;

		stat(tree[i]->d_name,&tstat);
		
		if(count==0)//처음자식일경우
		{
			printf("----");
			printf("%s",tree[i]->d_name);
		}
		else//그 다음 자식일 경우
		{
			if(count<num-1)
			{
				for(int j=0;j<level;j++)
				{
					printf("    |");
				}
			}
			else
			{
				for(int j=0;j<level;j++)
				{
					printf("    ");
				}
				printf("|");
			}
			printf("\n");
			for(int j=0;j<level;j++)
			{
				printf("    ");
			}
			printf("----%s",tree[i]->d_name);
		}
		if(S_ISDIR(tstat.st_mode))//디렉토리이면 다시 재귀
		{
			show_tree(tree[i]->d_name,level+1);//레벨이 증가한채로 재귀
			chdir("..");
			continue;
		}

		count++;
		printf("\n");
	}
	printf("\n");
	//chdir(dir);
}
/*데몬 프로세스 초기화 함수*/
int daemon_init(void){
	pid_t pid;
	int fd, maxfd;

	if((pid=fork())<0){//자식 프로세스 생성
		fprintf(stderr,"fork error\n");
		return -1;
	}
	else if(pid!=0)
		exit(0);

	setsid();
	//시그널 무시
	signal(SIGTTIN,SIG_IGN);
	signal(SIGTTOU,SIG_IGN);
	signal(SIGTSTP,SIG_IGN);
	maxfd=getdtablesize();

	//디몬 프로세스 실행
	execute_daemon(checkpath);

	return 0;
}

/*데몬 프로세스 실행 함수*/
void execute_daemon(char *checkDir)
{
	int pre_fsize=0;//바뀌기이전 파일사이즈
	int now_fsize=0;//바뀐 이후 파일사이즈
	FILE *fp;
	DIR *dp;
//	int totalfile1=0;
//	int totalfile2=0;

	chdir(savedpath);
	while(1){//"check"디렉터리의 총 파일 수 비교
		//파일의 기존 정보 info 구조체에 저장
		int num1=0;//처음파일의 개수
		int num2=0;//나중파일의 개수
		file_info file_info1[BUFLEN];//메모리 할당
		indexnum=0;//구조체에 담기는 파일 인덱스 초기화
		pre_fsize = 0;
		num1=file_scandir(checkpath,file_info1,&pre_fsize);//디렉토리안 파일개수

		sleep(1);

		file_info file_info2[BUFLEN];
		indexnum=0;//구조체에 담기는 파일 인덱스 초기화
		now_fsize = 0;
		num2=file_scandir(checkpath,file_info2,&now_fsize);//디렉토리 안 파일개수

		if(pre_fsize!=now_fsize)//파일사이즈가 다를때. 디렉터리에 변화가 생김
			checkfile(file_info1,file_info2,num1,num2);//검사함수 실행
		//이전디렉토리정보와 현재디렉토리정보를 비교하여 삭제,추가,수정 판단
	}
	return;
}

int file_scandir(char *dir,file_info *file_info,int* totalfile)
{
	struct dirent **items;
	struct stat fstat;
	int i;
	int num;
	int n=0;

	chdir(dir);//디렉토리 변경
	n=scandir(".",&items,NULL,alphasort);//디렉토리의 파일 개수
	for(i=0;i<n;i++)
	{
		if((items[i]->d_name)[0]=='.')
			continue;
		lstat(items[i]->d_name,&fstat);
		
		if(S_ISDIR(fstat.st_mode))//해당 파일이 디렉토리일 경우
		{
			file_scandir(items[i]->d_name,file_info,totalfile);
			//재귀해서 디렉토리 안 파일개수까지 받는다.
			continue;//새로운 디렉토리를 만났을시에 다시 처음으로 돌아가서 그 안의 파일들을 구조체에 입력
		}
		//구조체에 정보저장
		strcpy(file_info[indexnum].name,items[i]->d_name);//파일 이름 구조체에 담기
		file_info[indexnum].m_time=fstat.st_mtime;//파일 최종수정시간 구조체에 담기
		*totalfile+=fstat.st_size;//파일사이즈측정
		indexnum++;//파일개수측정
	}
	return indexnum;//총 파일개수 리턴
}

/*데몬프로세스에 의해서 log.txt에 로그찍는 함수*/
void checkfile(file_info *file_info1,file_info *file_info2,int num1,int num2){
	FILE *fp;
	time_t nowtime;
	struct tm *timeinfo;
	struct stat fstatbuf;
	char timebuf[BUFLEN];
	char filename[BUFLEN];
	int num;
	int i,j,k;

	chdir(savedpath);

	fp=fopen("log.txt","a+");

	chdir(checkpath);

	if(num2<num1)//"delete"일 경우
	{//파일개수가 줄었을 경우
		for(i=0;i<num2;i++)
		{
			if(!strcmp(file_info1[i].name,file_info2[i].name))//같을때는 계속
				continue;
			else//다를경우. 이게 삭제된 파일임
			{
				strcpy(filename,file_info1[i].name);
				break;
			}
		}
		if(i==num2)//마지막까지 발견되지 않으면 처음 파일 정보의 마지막이 해당 파일
		{
			strcpy(filename,file_info1[num1-1].name);
		}
		/*시간측정*/
		time(&nowtime);
		timeinfo=localtime(&nowtime);//현재시간이 삭제시간이 됨
		strftime(timebuf,BUFLEN,"%F %H:%M:%S",timeinfo);
		fprintf(fp,"[%s][delete_%s]\n",timebuf,filename);
	}
	else if(num2>num1)//"create"일 경우
	{//파일 개수가 늘었을 경우

		for(i=0;i<num1;i++)
		{
			if(!strcmp(file_info1[i].name,file_info2[i].name))//같을 때는 계속
				continue;
			else
			{//다르면 그 파일이 추가된 파일
				strcpy(filename,file_info2[i].name);
				break;
			}
		}
		if(i==num1)//마지막까지 발견안되면 새로운 파일구조체에서 마지막 파일이 추가된파일이 된다
			strcpy(filename,file_info2[num2-1].name);
		/*시간측정*/
		stat(filename,&statbuf);
		timeinfo=localtime(&statbuf.st_mtime);//해당파일의 최종수정시간이 생성시간이 됨
		strftime(timebuf,BUFLEN,"%F %H:%M:%S",timeinfo);
		fprintf(fp,"[%s][create_%s]\n",timebuf,filename);
	}
	else//"modify" 일 경우
	{//파일의 개수는 같으나 파일의 최종수정시간이 다름
		for(i=0;i<num1;i++)
		{
			if(file_info1[i].m_time!=file_info2[i].m_time)
			{//수정시간이 다른 파일 찾기
				strcpy(filename,file_info2[i].name);
				/*시간측정*/
				timeinfo=localtime(&file_info2[i].m_time);//해당파일의 최종수정시간이 변경시간이 됨
				strftime(timebuf,BUFLEN,"%F %H:%M:%S",timeinfo);
				fprintf(fp,"[%s][modify_%s]\n",timebuf,filename);
				break;
			}
			else //발견할때까지 계속
				continue;
		}
	}
	
	fclose(fp);
	return;
}

/*사용법 출력함수*/
void print_usage()
{
	printf("Usage : 20182620 [OPTION]\n");
	printf("Option : \n");
	printf("delete [FILENAME] [END_TIME] [OPTION] | delete file in directory\n");
	printf("size [FILENAME] [OPTION] | print size on file\n");
	printf("recover [FILENAME] [OPTION] | recover file from file\n");
	printf("tree | print tree for directory\n");
	printf("exit | exit program\n");
	printf("help | print usage\n");
}

