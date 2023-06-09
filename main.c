#include "libmalloc.h"
#include <stdio.h>
#include <pthread.h>


const int n = 9; // N
void *pointer[9];
static pthread_mutex_t output_file, printf_mut;

void *make_blocks(void *argc){
	int i = *((int *)argc);
	pointer[i * 3] = libmalloc(16);
	pointer[i * 3 + 1] = libmalloc(1024);
	pointer[i * 3 + 2] = libmalloc(4096);
	pthread_mutex_lock(&printf_mut);
	printf("%p %p %p\n", pointer[i * 3], pointer[i * 3 + 1], pointer[i * 3 + 2]);
	pthread_mutex_unlock(&printf_mut);
}

void *fill_blocks(void *argc){
	int i = *((int *)argc) % (n / 3);
	int j = 0;
	int **list = (int **)pointer;
	for(j = 0; j < 16 / sizeof(int); j += sizeof(int)){
		list[i * 3][j] = *((int *)argc);
	}
	for(j = 0; j < 1024 / sizeof(int); j += sizeof(int)){
		list[i * 3 + 1][j] = *((int *)argc);
	}
	for(j = 0; j < 4096 / sizeof(int); j += sizeof(int)){
		list[i * 3 + 2][j] = *((int *)argc);
	}
}

void *output_info(void *argc) {
	int i = *((int *)argc) % (n / 3);
	pthread_mutex_lock(&output_file);
	FILE *file = fopen("log.txt", "a");
	fprintf(file, "%p %p %p\n",  pointer[i * 3], pointer[i * 3 + 1], pointer[i * 3 + 2]);
	fclose(file);
	libfree(pointer[i * 3]);
	libfree(pointer[i * 3 + 1]);
	libfree(pointer[i * 3 + 2]);
	pthread_mutex_unlock(&output_file);
}

int main(){
	pthread_t threads[n];
	int i = 0;
	for(i = 0; i < n; ++i){
		if(i < n / 3){
			pthread_create(threads + i, NULL, make_blocks, (void*)&i);
			pthread_join(threads[i], NULL);
		}else if(i < 2 * n / 3){
			pthread_create(threads + i, NULL, fill_blocks, (void*)&i);
			pthread_join(threads[i], NULL);
		}else{
			pthread_create(threads + i, NULL, output_info, (void*)&i);
			pthread_join(threads[i], NULL);
		}
	}
}