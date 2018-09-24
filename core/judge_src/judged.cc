/*
* Copyright 2008 sempr <iamsempr@gmail.com>
*
* Refacted and modified by zhblue<newsclan@gmail.com>
* Bug report email newsclan@gmail.com
*
* This file is part of HUSTOJ.
*
* HUSTOJ is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* HUSTOJ is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with HUSTOJ. if not, see <http://www.gnu.org/licenses/>.
*/
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <mysql/mysql.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/resource.h>
static int DEBUG = 0; //�Ƿ����õ��ԣ����鿴��־���м�¼��Ĭ��0��������
#define BUFFER_SIZE 1024
#define LOCKFILE "/var/run/judged.pid"
#define LOCKMODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)
#define STD_MB 1048576

#define OJ_WT0 0
#define OJ_WT1 1
#define OJ_CI 2
#define OJ_RI 3
#define OJ_AC 4
#define OJ_PE 5
#define OJ_WA 6
#define OJ_TL 7
#define OJ_ML 8
#define OJ_OL 9
#define OJ_RE 10
#define OJ_CE 11
#define OJ_CO 12

static char host_name[BUFFER_SIZE];
static char user_name[BUFFER_SIZE];
static char password[BUFFER_SIZE];
static char db_name[BUFFER_SIZE];
static char oj_home[BUFFER_SIZE];
static char oj_lang_set[BUFFER_SIZE];
static int port_number;
static int max_running;
static int sleep_time;
static int sleep_tmp;
static int oj_tot;
static int oj_mod;
static int http_judge = 0;
static char http_baseurl[BUFFER_SIZE];
static char http_username[BUFFER_SIZE];
static char http_password[BUFFER_SIZE];

static bool STOP = false;

static MYSQL *conn;
static MYSQL_RES *res;	//mysql��ȡ���������_get_http/mysql_jobs()�б�����
static MYSQL_ROW row;
//static FILE *fp_log;
static char query[BUFFER_SIZE];//��init_mysql_conf�и��£��̶�ȡ2���������ͻ��˵Ĵ�������Ŀsolution_id

void call_for_exit(int s) {
	STOP = true;
	printf("Stopping judged...\n");
}

void write_log(const char *fmt, ...) {
	va_list ap;
	char buffer[4096];
	//	time_t          t = time(NULL);
	//	int             l;
	sprintf(buffer, "%s/log/client.log", oj_home);
	FILE *fp = fopen(buffer, "a+");
	if (fp == NULL) {
		fprintf(stderr, "openfile error!\n");
		system("pwd");
	}
	va_start(ap, fmt);
	vsprintf(buffer, fmt, ap);
	fprintf(fp, "%s\n", buffer);
	if (DEBUG)
		printf("%s\n", buffer);
	va_end(ap);
	fclose(fp);

}

int after_equal(char * c) {
	int i = 0;
	for (; c[i] != '\0' && c[i] != '='; i++)
		;
	return ++i;
}
void trim(char * c) {
	char buf[BUFFER_SIZE];
	char * start, *end;
	strcpy(buf, c);
	start = buf;
	while (isspace(*start))
		start++;
	end = start;
	while (!isspace(*end))
		end++;
	*end = '\0';
	strcpy(c, start);
}
bool read_buf(char * buf, const char * key, char * value) {
	if (strncmp(buf, key, strlen(key)) == 0) {
		strcpy(value, buf + after_equal(buf));
		trim(value);
		if (DEBUG)
			printf("%s\n", value);
		return 1;
	}
	return 0;
}
void read_int(char * buf, const char * key, int * value) {
	char buf2[BUFFER_SIZE];
	if (read_buf(buf, key, buf2))
		sscanf(buf2, "%d", value);

}
// read the configue file
void init_mysql_conf() {
	FILE *fp = NULL;
	char buf[BUFFER_SIZE];
	host_name[0] = 0;
	user_name[0] = 0;
	password[0] = 0;
	db_name[0] = 0;
	port_number = 3306;
	max_running = 3;
	sleep_time = 1;
	oj_tot = 1;
	oj_mod = 0;
	strcpy(oj_lang_set, "0,1,2,3,4,5,6,7,8,9,10");
	fp = fopen("./etc/judge.conf", "r");
	if (fp != NULL) {
		while (fgets(buf, BUFFER_SIZE - 1, fp)) {
			read_buf(buf, "OJ_HOST_NAME", host_name);
			read_buf(buf, "OJ_USER_NAME", user_name);
			read_buf(buf, "OJ_PASSWORD", password);
			read_buf(buf, "OJ_DB_NAME", db_name);
			read_int(buf, "OJ_PORT_NUMBER", &port_number);
			read_int(buf, "OJ_RUNNING", &max_running);
			read_int(buf, "OJ_SLEEP_TIME", &sleep_time);
			read_int(buf, "OJ_TOTAL", &oj_tot);

			read_int(buf, "OJ_MOD", &oj_mod);

			read_int(buf, "OJ_HTTP_JUDGE", &http_judge);
			read_buf(buf, "OJ_HTTP_BASEURL", http_baseurl);
			read_buf(buf, "OJ_HTTP_USERNAME", http_username);
			read_buf(buf, "OJ_HTTP_PASSWORD", http_password);
			read_buf(buf, "OJ_LANG_SET", oj_lang_set);

		}
		sprintf(query,
			"SELECT solution_id FROM solution WHERE language in (%s) and result<2 and MOD(solution_id,%d)=%d ORDER BY result ASC,solution_id ASC limit %d",
			oj_lang_set, oj_tot, oj_mod, max_running * 2);
		sleep_tmp = sleep_time;
		//	fclose(fp);
	}
}


//���д������ύ�����ҽ��������������£������µ��ӽ��̵��ø����⺯��
//���룺�������ύ��solution_id, �ӽ�����ID[]�еı���λ�� i  
void run_client(int runid, int clientid) {
	char buf[BUFFER_SIZE], runidstr[BUFFER_SIZE];
	//��Linuxϵͳ�У�Resouce limitָ��һ�����̵�ִ�й����У������ܵõ�����Դ�����ƣ�
	//������̵�core file�����ֵ�������ڴ�����ֵ�� ����������ʱ�䣬�ڴ��Сʵ�ֵĹؼ� 
	/*
	�ṹ���� rlim_cur��Ҫȡ�û����õ���Դ�����Ƶ�ֵ��rlim_max��Ӳ����
	������ֵ��������һ��С��Լ����
	1�� �κν��̿��Խ������Ƹ�ΪС�ڻ����Ӳ����
	2���κν��̶����Խ�Ӳ���ƽ��ͣ�����ͨ�û������˾��޷���ߣ���ֵ������ڻ����������
	3�� ֻ�г����û��������Ӳ����

	setrlimit(int resource,const struct rlimit rlptr);���أ����ɹ�Ϊ0������Ϊ��0
	RLIMIT_CPU��CPUʱ��������ֵ���룩����������������ʱ��ý��̷���SIGXCPU�ź�
	RLIMIT_FSIZE:���Դ������ļ�������ֽڳ��ȣ���������������ʱ����̷���SIGXFSZ
	*/
	struct rlimit LIM;
	LIM.rlim_max = 800;
	LIM.rlim_cur = 800;
	setrlimit(RLIMIT_CPU, &LIM);//cpu����ʱ������ 

	LIM.rlim_max = 80 * STD_MB;
	LIM.rlim_cur = 80 * STD_MB;
	setrlimit(RLIMIT_FSIZE, &LIM);//���ļ���С���ƣ���ֹ���������� 

	LIM.rlim_max = STD_MB << 11;//����11 STD_MB��2^20 MB 2^11MB 2GB���������2GB�����ڴ棿 
	LIM.rlim_cur = STD_MB << 11;
	setrlimit(RLIMIT_AS, &LIM);//������е������ڴ��С���� 

	LIM.rlim_cur = LIM.rlim_max = 200;
	setrlimit(RLIMIT_NPROC, &LIM);//ÿ��ʵ���û�ID��ӵ�е�����ӽ���������Щ����Ϊ�˷�ֹ�������İɣ��� 

								  //buf[0]=clientid+'0'; buf[1]=0;
	sprintf(runidstr, "%d", runid);//ת�����ַ��������ַ����� 
	sprintf(buf, "%d", clientid);

	//write_log("sid=%s\tclient=%s\toj_home=%s\n",runidstr,buf,oj_home);
	//sprintf(err,"%s/run%d/error.out",oj_home,clientid);
	//freopen(err,"a+",stderr);

	if (!DEBUG)
		execl("/usr/bin/judge_client", "/usr/bin/judge_client", runidstr, buf,
			oj_home, (char *)NULL);
	else

		//����ֵ�����ִ�гɹ��������᷵��, ִ��ʧ����ֱ�ӷ���-1, ʧ��ԭ�����errno ��. 
		//execl()���к�׺"l"����listҲ���ǲ����б����˼����һ����path�ַ�ָ����ָ��Ҫִ�е��ļ�·���� 
		//�������Ĳ�������ִ�и��ļ�ʱ���ݵĲ����б�argv[0],argv[1]... ���һ���������ÿ�ָ��NULL�������� 
		//	ִ��/binĿ¼�µ�ls, ��һ����Ϊ������ls, �ڶ�������Ϊ"-al", ����������Ϊ"/etc/passwd"
		//execl("/bin/ls", "ls", "-al", "/etc/passwd", (char *) 0);
		//�����һ������Ϊ��������judge_client���ڶ�������Ϊ��������Ŀid, ������Ϊ������pid����λ�ã����ĸ�����ΪojĿ¼
		//Ĭ��/home/judge,���������Ϊ��debug�� 
		execl("/usr/bin/judge_client", "/usr/bin/judge_client", runidstr, buf,
			oj_home, "debug", (char *)NULL);

	//exit(0);
}
//ִ��sql���ɹ�����1�����򷵻�0 
//���ҹر��Ƿ�conn������init���ʼ����ʼ�� 
int executesql(const char * sql) {

	if (mysql_real_query(conn, sql, strlen(sql))) {
		if (DEBUG)
			write_log("%s", mysql_error(conn));
		sleep(20);
		conn = NULL;
		return 1;
	}
	else
		return 0;
}

int init_mysql() {
	if (conn == NULL) {
		conn = mysql_init(NULL);		// init the database connection
										/* connect the database */
		const char timeout = 30;
		mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

		if (!mysql_real_connect(conn, host_name, user_name, password, db_name,
			port_number, 0, 0)) {
			if (DEBUG)
				write_log("%s", mysql_error(conn));
			sleep(2);
			return 1;
		}
		else {
			return 0;
		}
	}
	else {
		return executesql("set names utf8");
	}
}
FILE * read_cmd_output(const char * fmt, ...) {
	char cmd[BUFFER_SIZE];

	FILE * ret = NULL;
	va_list ap;

	va_start(ap, fmt);
	vsprintf(cmd, fmt, ap);
	va_end(ap);
	//if(DEBUG) printf("%s\n",cmd);
	ret = popen(cmd, "r");

	return ret;
}
int read_int_http(FILE * f) {
	char buf[BUFFER_SIZE];
	fgets(buf, BUFFER_SIZE - 1, f);
	return atoi(buf);
}
bool check_login() {
	const char * cmd =
		"wget --post-data=\"checklogin=1\" --load-cookies=cookie --save-cookies=cookie --keep-session-cookies -q -O - \"%s/admin/problem_judge.php\"";
	int ret = 0;

	FILE * fjobs = read_cmd_output(cmd, http_baseurl);
	ret = read_int_http(fjobs);
	pclose(fjobs);

	return ret > 0;
}
void login() {
	if (!check_login()) {
		char cmd[BUFFER_SIZE];
		sprintf(cmd,
			"wget --post-data=\"user_id=%s&password=%s\" --load-cookies=cookie --save-cookies=cookie --keep-session-cookies -q -O - \"%s/login.php\"",
			http_username, http_password, http_baseurl);
		system(cmd);
	}

}
int _get_jobs_http(int * jobs) {
	login();
	int ret = 0;
	int i = 0;
	char buf[BUFFER_SIZE];
	const char * cmd =
		"wget --post-data=\"getpending=1&oj_lang_set=%s&max_running=%d\" --load-cookies=cookie --save-cookies=cookie --keep-session-cookies -q -O - \"%s/admin/problem_judge.php\"";
	FILE * fjobs = read_cmd_output(cmd, oj_lang_set, max_running, http_baseurl);
	while (fscanf(fjobs, "%s", buf) != EOF) {
		//puts(buf);
		int sid = atoi(buf);
		if (sid > 0)
			jobs[i++] = sid;
		//i++;
	}
	pclose(fjobs);
	ret = i;
	while (i <= max_running * 2)
		jobs[i++] = 0;
	return ret;
	return ret;
}
//���ܣ�ȡ�ô�������Ŀ��Ϣ��jobs����
//���룺int * jobs :����solution_id/runid
//����������ѯ�ɹ��򷵻أ�Ҫ������Ŀ���� 
//�����ѯ������Ŀ���ɹ��򷵻�0

int _get_jobs_mysql(int * jobs) {
	//mysql.h
	//�����ѯ���ݰ��������ƻ��߸����ٶ� �����
	//���ִ�гɹ�������0�����ɹ���0
	if (mysql_real_query(conn, query, strlen(query))) {
		if (DEBUG)
			write_log("%s", mysql_error(conn));
		sleep(20);
		return 0;
	}

	//mysql.h
	//���ؾ��ж�������MYSQL_RES������ϡ�������ִ��󣬷���NULL
	//����μ��ٶ�
	res = mysql_store_result(conn);
	int i = 0;
	int ret = 0;
	//���������mysql_fetch_row()
	while ((row = mysql_fetch_row(res)) != NULL) {
		jobs[i++] = atoi(row[0]);
	}
	ret = i; //Ҫ����jobsĩ�� �� 0 1 2 �����ݣ���i=3��������
	while (i <= max_running * 2)
		jobs[i++] = 0; //�趨���������ĿΪmax_running*2����0-8��λ0��9�� max_running*2+1���鿪��ô�� 
	return ret;
	return ret;
}
int get_jobs(int * jobs) {
	if (http_judge) {	//web��coreĬ�����ӷ�ʽ�����ݿ⣬web����solution,core��ѵ/����solution-result��web��ѵsolution-result
		return _get_jobs_http(jobs);
	}
	else
		return _get_jobs_mysql(jobs);//��ȡҪ�������������

}

//���³�ʼ��solution���
//���³ɹ�����1������0
// ���ʣ�OJ_CIΪ2��and result < 2�����ô���ǲ����������Sql�����ô������ִ�гɹ��Ŷ԰� 
//��limit 1����һ�㱣�ϡ�����where ���������쳣ʱ���������Ӱ��̫�ࡣ 
//��֪��php��ʼд���٣����ǵ��ø��Ĳ���Ϊ2���������������� 
bool _check_out_mysql(int solution_id, int result) {
	char sql[BUFFER_SIZE]; //sql��䱣�� 
	sprintf(sql,
		"UPDATE solution SET result=%d,time=0,memory=0,judgetime=NOW() WHERE solution_id=%d and result<2 LIMIT 1",
		result, solution_id);
	//ִ��sql��䣬�ɹ�����0�������0 
	if (mysql_real_query(conn, sql, strlen(sql))) {
		syslog(LOG_ERR | LOG_DAEMON, "%s", mysql_error(conn));
		return false;
	}
	else {
		//Ӱ������������������0��ִ�гɹ�������1������0 
		if (mysql_affected_rows(conn) > 0ul)
			return true;
		else
			return false;
	}

}

bool _check_out_http(int solution_id, int result) {
	login();
	const char * cmd =
		"wget --post-data=\"checkout=1&sid=%d&result=%d\" --load-cookies=cookie --save-cookies=cookie --keep-session-cookies -q -O - \"%s/admin/problem_judge.php\"";
	int ret = 0;
	FILE * fjobs = read_cmd_output(cmd, solution_id, result, http_baseurl);
	fscanf(fjobs, "%d", &ret);
	pclose(fjobs);

	return ret;
}

//��ʼ����solution��
//���ݲ�����ִͬ�в�ͬ�ĸ��º��� 
bool check_out(int solution_id, int result) {

	if (http_judge) {
		return _check_out_http(solution_id, result);
	}
	else
		return _check_out_mysql(solution_id, result);

}
int work() {
	//      char buf[1024];
	static int retcnt = 0;//ͳ�� �Ѿ� ����������  
	int i = 0;
	static pid_t ID[100];  //short���͵ĺ궨�壬���̱��е���������̺ţ���������ִ�е��ӽ���pid 
	static int workcnt = 0;//ͳ�� ���� judge_client�������� 
	int runid = 0;			//solution_id���������б��
	int jobs[max_running * 2 + 1];//max_running ��judge.conf��ȡ��һ��Ϊ4����������Ϊ����Ŀ¼��9
	pid_t tmp_pid = 0;

	//for(i=0;i<max_running;i++){
	//      ID[i]=0;
	//}

	//sleep_time=sleep_tmp;
	/* get the database info */
	if (!get_jobs(jobs)) //�����ȡʧ�ܻ���Ҫ������Ŀ����Ϊ0��jobs[]����Ϊ��1001��1002��0��...0��Ĭ��9λ 
		retcnt = 0;
	/* exec the submit *///��������ÿ��solution_id����Ŀ��ֻ�����������Ŀȫ��Ͷ�뵽�µ����н�����
						 //�����Ƿ�������� 
	for (int j = 0; jobs[j] > 0; j++) {
		runid = jobs[j]; //��ȡsolution_id���������ύ��Ŀid 
						 //��ʽ���������У�Ĭ��oj_tot Ϊ 1 oj_mod Ϊ0����init_sql_conf������ �������� 
		if (runid % oj_tot != oj_mod)
			continue;
		if (DEBUG) //������Ĭ��0 ���� 
			write_log("Judging solution %d", runid);
		//workcnt Ϊstatic �������൱��������ͳ������run_client���� ��Ŀ 
		//��if �ȴ����� �ӽ��̣������� i �ڳ����� ���ӽ��̵�λ�� 
		if (workcnt >= max_running) {           // if no more client can running
												//����ﵽ�˿�����������Ŀ����ô�ȴ�һ���ӽ��̽���
												//waitpid���ο�linux �� c ���Ա���µ� ���̹��� 
												//waitpid()����ʱֹͣĿǰ���̵�ִ�У�ֱ�����ź��������ӽ��̽���
												//pid_t waitpid(pid_t pid,int * status,int options);
												//pid=-1 ���������ӽ��̣�status ȡ���ӽ���ʶ���룬���ﲻ��Ҫ����NULL; 
												//����options�ṩ��һЩ�����ѡ��������waitpid�����粻�ȴ�����ִ�У�����0����ʹ�ã����̹���
												//��� ���ӽ����Ѿ���������ôִ�е������ʱ���ֱ���������ӽ���Ҳ���ɽ�ʬ�����ͷ�	
												//���ؽ������ӽ���pid	 
			tmp_pid = waitpid(-1, NULL, 0);     // wait 4 one child exit
			workcnt--;//�ӽ��̽����˸�����ô����judge_client������һ  
			retcnt++;//�����������1 
					 //��������� ID[]����Ѿ��������ӽ�����Ϣ 
			for (i = 0; i < max_running; i++)     // get the client id
				if (ID[i] == tmp_pid)
					break; // got the client id
			ID[i] = 0;
		}
		else {                                             // have free client

			for (i = 0; i < max_running; i++)     // find the client id
				if (ID[i] == 0)
					break;    // got the client id
		}

		//��ʵ����worknct<max_running һ������������waitpid()���� 
		//check_out:���³�ʼ����������ô������ִ�гɹ��ŶԵİ���Ϊʲô���ܳɹ���
		//������Կ�ʼ�µ��ӽ��̽������� 
		if (workcnt < max_running && check_out(runid, OJ_CI)) {
			workcnt++;//�������ӽ�����Ŀ��1----�����ǲ���̫���ˣ��ӽ��̴���һ���ܳɹ�����������
					  //Ӧ�����ӽ�������������ֵ�� 
			ID[i] = fork();   //�����ӽ��� �����ӽ���pid���ظ������̣���0���ظ��ӽ���  // start to fork
							  //���д�ľ�������⣬�����̻Ὣ�����Ϊ�½���pid
							  //�ӽ����أ�����֮�������Ϊ0���ǵ����Ƕ��٣�������������
							  //���ճ����ӽ��̻��������Ƹ����̵Ĵ��룬���ݣ���ջ
							  //��ô����Ǹ�������ִ����ôID[i] ��Ϊ0�����ӽ���pid
							  //������ӽ��̵���ִ�У���ô���ݶ�����ID[i]Ϊ0��������
							  //��static �������� 
			if (ID[i] == 0) {//�����������ô��������ִ���ӽ��̴��룬ִ��run_judge_client 
				if (DEBUG)
					write_log("<<=sid=%d===clientid=%d==>>\n", runid, i);
				run_client(runid, i);  //���ӽ��������ID[0]=pid  // if the process is the son, run it
				exit(0);//�ӽ���ִ������˳�0�������̲���ִ�����if ����run_client����̻���ת��execl(judge_client)
						//ִ�гɹ������أ����ɹ����ط�0��������erro���ô����������ôִ�е��ģ��ӽ�������˳��ģ����������� 
			}

		}
		else {//�����ϣ����ϸ�if���Ѿ���֤������ΪID[i] = 0�����������Ϊ�˽�һ����֤ 
			ID[i] = 0;
		}
	}

	//�ѱ�����ѵ���Ĵ�������Ŀȫ��Ͷ������� 
	//�ڷǹ���ȴ��ӽ��̵Ľ�����������ӽ���������ɽ��� 
	//���ϸ���for������ý���û�е�ʱ����ô�ͱ��������һ�����̽�������ô���ܼ���ִ�У�������for���Ѿ��� 
	// �ӽ��̽����ǽ�ʬ�����ˣ�ֻҪworkcnt<max_running,��ô��Ҳ�������ӽ�ʬ���̵Ļ������⣬��������Ͷ���µ��ӽ���
	//��������
	//��ô�ӽ�ʬ���� ˭�����գ���ʱ���գ���ô���գ��ܲ��ܵȿ��õ�ȫ���˽�ʬ���̣���for���õ���ʱ���ڽ��л��հ� 
	//������ý����������ر�󣬶�һֱû���û��ύ�������ǣ�����һ��ʱ���ϵͳ�϶���һֱ��max_running�����̵�
	//��Դ��ռ�ã����Ҵ���99%���ֶ��������ӽ�ʬ���̣�forֻ���ü����ռ�����������Ҳû���������ģ���Ϊforֻ�е�
	//�����������ʱ��Ż�ִ�е����󲿷�û���û��ύ�����������ѯʱ������˳�ֻ����£��񲻿�ϧ������ 

	//���Ծ���while()Ҫ��ɵ����񣬸�����ִ�е������ʱ��ɨһ���Ƿ��д������ӽ�ʬ���̣��о� ˳�ֻ���һ����
	// ��Ϊ��֪���ж��ٴ����յģ�ʲôʱ��Ҫ���գ�����ֻ��ֻ���������ѯʱ��������һ�� 
	//���while��������˳��ǣ����Ϊ����ȻҲ�и������������������Ҫ����~~~ 
	/*
	���ʹ����WNOHANG��������waitpid����ʹû���ӽ����˳�����Ҳ���������أ�������wait������Զ����ȥ
	1�����������ص�ʱ��waitpid�����ռ������ӽ��̵Ľ���ID��
	2�����������ѡ��WNOHANG����������waitpid����û�����˳����ӽ��̿��ռ����򷵻�0��
	3����������г����򷵻�-1����ʱerrno�ᱻ���ó���Ӧ��ֵ��ָʾ�������ڣ�
	*/
	while ((tmp_pid = waitpid(-1, NULL, WNOHANG)) > 0) {
		workcnt--;
		retcnt++;
		for (i = 0; i < max_running; i++)     // get the client id
			if (ID[i] == tmp_pid)
				break; // got the client id
		ID[i] = 0;
		printf("tmp_pid = %d\n", tmp_pid);
	}
	//�ͷ����ݿ���Դ 
	//����commit�ĵ��ã���֪����Ϊ�˹ر�conn���������ݿⲻ֧���Զ�commit
	//���ǳ�����С��־����������rollback����ѧϰ������������ 
	if (!http_judge) {
		mysql_free_result(res);                         // free the memory
		executesql("commit");
	}
	if (DEBUG && retcnt)
		write_log("<<%ddone!>>", retcnt);
	//free(ID);
	//free(jobs);
	//�����Ѿ�����Ĵ��� 
	return retcnt;
}

int lockfile(int fd) {
	struct flock fl;
	fl.l_type = F_WRLCK;
	fl.l_start = 0;
	fl.l_whence = SEEK_SET;
	fl.l_len = 0;
	return (fcntl(fd, F_SETLK, &fl));
}

int already_running() {
	int fd;
	char buf[16];
	fd = open(LOCKFILE, O_RDWR | O_CREAT, LOCKMODE);
	if (fd < 0) {
		syslog(LOG_ERR | LOG_DAEMON, "can't open %s: %s", LOCKFILE,
			strerror(errno));
		exit(1);
	}
	if (lockfile(fd) < 0) {
		if (errno == EACCES || errno == EAGAIN) {
			close(fd);
			return 1;
		}
		syslog(LOG_ERR | LOG_DAEMON, "can't lock %s: %s", LOCKFILE,
			strerror(errno));
		exit(1);
	}
	ftruncate(fd, 0);
	sprintf(buf, "%d", getpid());
	write(fd, buf, strlen(buf) + 1);
	return (0);
}
int daemon_init(void)

{
	pid_t pid;

	if ((pid = fork()) < 0)
		return (-1);

	else if (pid != 0)
		exit(0); /* parent exit */

				 /* child continues */

	setsid(); /* become session leader */

	chdir(oj_home); /* change working directory */

	umask(0); /* clear file mode creation mask */

	close(0); /* close stdin */

	close(1); /* close stdout */

	close(2); /* close stderr */

	return (0);
}

int main(int argc, char** argv) {
	DEBUG = (argc > 2);
	if (argc > 1)
		strcpy(oj_home, argv[1]);
	else
		strcpy(oj_home, "/home/judge");
	chdir(oj_home);    // change the dir

	if (!DEBUG)
		daemon_init();//�ǵ���ģʽ����һ���ػ�����
	if (strcmp(oj_home, "/home/judge") == 0 && already_running()) {
		syslog(LOG_ERR | LOG_DAEMON,
			"This daemon program is already running!\n");
		return 1;
	}
	//	struct timespec final_sleep;
	//	final_sleep.tv_sec=0;
	//	final_sleep.tv_nsec=500000000;
	init_mysql_conf();	// set the database info
	signal(SIGQUIT, call_for_exit);
	signal(SIGKILL, call_for_exit);
	signal(SIGTERM, call_for_exit);
	int j = 1;
	while (1) {			// start to run
						//���while�ĺô����ڣ�ֻҪһ�������ץ��ռ��ϵͳ���Ȱ�������������ɣ����»��ѭ�����εĿ��ܴ���
						//����û������󣬾ͻ���뵽����ɢ���� ��Ϣsleep(time)������ѯ�ǲ����������ͷ�ϵͳ����Դ������Damonһֱ
						//��ѭ��ռ��ϵͳ 
		while (j && (http_judge || !init_mysql())) {

			j = work();//�����ȡʧ�ܻ���û��Ҫ��������ݣ���ô����0��������ô���޵ļ����������������޵������� 

		}
		sleep(sleep_time);
		j = 1;
	}
	return 0;
}
