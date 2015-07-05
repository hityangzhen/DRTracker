#include <iostream>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <numeric>
using namespace std;

static const int max_len=100;
static unsigned int rss_array[max_len];
static int i=0;

pid_t get_pid(const char *proc)
{
	//open the pipe
	char cmd[100];
	sprintf(cmd,"ps -e | grep \'%s\' | awk \'{print $1}\'",proc);
	FILE *pp = popen(cmd, "r");
	if (!pp) {
        return -1;
    }
	//get the process pid
	char buffer[10];
	while (NULL != fgets(buffer, 10, pp)) {
		break;
    }
    pclose(pp);
    return atoi(buffer);
}

unsigned int mem_overhead(pid_t pid)
{
	//open the pipe
	char cmd[30];
	sprintf(cmd,"ps -p %d -o rss",pid);
	char buffer[15];
	FILE *pp=NULL;
	while(true) {
		pp = popen(cmd, "r");
		if (!pp) {
        	return -1;
    	}
    	fgets(buffer,15,pp);
    	if(fgets(buffer,15,pp)==NULL || (rss_array[i]=atoi(buffer))==0)
    		break;
    	memset(buffer, 0,sizeof(buffer));
    	i=(i+1)%max_len;
    	pclose(pp);
    	sleep(1);
	}
	unsigned int sum=accumulate(&rss_array[0],&rss_array[max_len-1],0);
	if(rss_array[max_len-1]==0)
		return sum/(i+1);
	return sum/max_len;
}

int main(int argc,char *argv[])
{
	if(argc!=2) {
		cout<<"only one argument"<<endl;
		return 0;
	}
	pid_t pid=get_pid(argv[1]);
	cout<<argv[1]<<" memory average overhead: "<<mem_overhead(pid)<<endl;
	return 0;
}