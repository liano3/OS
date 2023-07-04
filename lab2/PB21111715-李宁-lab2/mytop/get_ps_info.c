#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/syscall.h>
#define MAX 1000
int main(int argc, char *argv[])
{
	int num = 0;
	__pid_t pid[MAX];
	char name[MAX][16];
	int state[MAX];
	__U64_TYPE runtime[MAX];
	
	int num_old = 0;	// 上一次的进程数
	__pid_t pid_old[MAX];
	__U64_TYPE runtime_old[MAX];	// 保存旧进程的 pid 和 runtime
	
	double cpu_usage[MAX];
	int table[MAX]; // 按占用率排序后的映射表
	
	int interval;
	if (argc == 1)
		interval = 1;
	else
		interval = atoi(argv[1]);
	
	syscall(332, &num_old, pid_old, name, state, runtime_old);
	while(1) {
		system("clear");
		syscall(332, &num, pid, name, state, runtime);
		for (int i = 0; i < num; i++) {
			int j;
			for (j = 0; j < num_old; j++) {
				if (pid[i] == pid_old[j]) {	// 保证同一进程
					cpu_usage[i] = (runtime[i] - runtime_old[j]) / (10000000.0 * interval);
					break;
				}
			}
			if (j >= num_old)
				cpu_usage[i] = runtime[i] / (10000000.0 * interval); // 新进程
			table[i] = i;
		}
		num_old = num; // 更新老进程
		for (int i = 0; i < num; i++) {	
			pid_old[i] = pid[i];
			runtime_old[i] = runtime[i];
		}
		// 排序获得占用率前20，第一个元素是占用率最大的
		printf("PID\t \tNAME\t \tSTATE\t \tCPU_USAGE(%%)\tRUNTIME(ns) \n");
		for (int i = 0; i < 20; i++) {
			for (int j = i + 1; j < num; j++) {
				if (cpu_usage[table[i]] < cpu_usage[table[j]]) {
					int tmp = table[i];
					table[i] = table[j];
					table[j] = tmp;
				}
			}
			printf("%-8d \t%-16s %-8d \t%-8lf \t%-8lu \n", pid[table[i]], name[table[i]], !state[table[i]], cpu_usage[table[i]], runtime[table[i]]);
		}
		sleep(interval);
	}
	return 0;
}
