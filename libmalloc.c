// fixCommits
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <pthread.h>

typedef struct s_block{
    size_t size;
    struct s_block *next;
    struct s_block *prev;
    int free;
}s_block;

static s_block *first_block = NULL;
const size_t SIZE_BLOCK = sizeof(s_block);
static pthread_mutex_t malloc_mutex;    

s_block* find_block(size_t size){
    s_block *cur_block = first_block;
    s_block *ans = NULL;
    int max_dif = -1;
    int flag = 1;
    while(cur_block && (cur_block != first_block || flag)){
        if(cur_block->free){
            if((cur_block->size > size && max_dif == -1) || (cur_block->size > size && cur_block->size - size < max_dif)){
                max_dif = cur_block->size - size;
                ans = cur_block;
            }
        }
        cur_block = cur_block->next;
        flag = 0;
    }

    return ans;
}

s_block* extend_new_block(size_t size){
    s_block *new_block;
    new_block = sbrk(0);

    if(sbrk(SIZE_BLOCK + size) == (void *)-1){
        return NULL;
    }

    new_block->size = size;
    new_block->free = 0;

    if(first_block){
        s_block *last_block = first_block->prev;
        last_block->next = new_block;
        new_block->prev = last_block;
        new_block->next = first_block;
        first_block->prev = new_block;
    }else{
        first_block = new_block;
        first_block->next = first_block;
        first_block->prev = first_block;
    }

    return new_block;
}

void split_block(s_block *block, size_t size){
    s_block *new_block;

    new_block = block + SIZE_BLOCK + size;

    new_block->size = block->size - size - SIZE_BLOCK;
    new_block->next = block->next;
    block->next = new_block;
    new_block->prev = block;

    block->size = size;
    new_block->free = 1;
}

size_t align8(size_t s) {
    if(s % 8 == 0){
       return s;
    }
 
    return ((s >> 3) + 1) << 3;
}

void *my_malloc(size_t size){
    s_block *block;
    size_t s = align8(size);

    if(first_block){
        block = find_block(s);
        if(block){
            if((block->size - s) >= (SIZE_BLOCK + 8)){
                split_block(block, s);
            }
            block->free = 0;
        }else{
           block = extend_new_block(s);
            if(!block){
                return NULL;
            } 
        }
    }else{
        block = extend_new_block(s);
        if(!block){
            return NULL;
        }
        first_block = block;
    }

    return ((void *)block) + SIZE_BLOCK;
}

void *libmalloc(size_t size){
    pthread_mutex_lock(&malloc_mutex);
    void *ans = my_malloc(size);
    pthread_mutex_unlock(&malloc_mutex);
    return ans;
}

void *my_calloc(size_t number, size_t size){
    size_t *new;
    size_t s8, i;
    new = my_malloc(number * size);
    if(new){
        s8 = align8(number * size) >> 3;

        for(i = 0; i < s8; i++){
            new[i] = 0;
        }
    }

    return new;
}

void *libcalloc(size_t number, size_t size){
    pthread_mutex_lock(&malloc_mutex);
    void *ans = my_calloc(number, size);
    pthread_mutex_unlock(&malloc_mutex);
    return ans;
}

int valid_addres(void *p){
    if(first_block){
        if(p >= (void *)first_block && p < sbrk(0)){
            int flag = 1, valid_flag = 0;
            s_block *cur_block = first_block;

            while(cur_block && (cur_block != first_block || flag)){
                if(cur_block == p - SIZE_BLOCK){
                    valid_flag = 1;
                    break;
                }
                cur_block = cur_block->next;
                flag = 0;
            }
            return valid_flag;
        }
    }
    return 0;
}

s_block *fusion(s_block *block){
    if(block->next != block && block->next->free){
        block->size += SIZE_BLOCK + block->next->size;
        block->next = block->next->next;
        if(block->next){
            block->next->prev = block;
        }
    }

    return block;
}

void my_free(void *p){
    s_block *block;
    if(valid_addres(p)){
        block = (s_block *)(p - SIZE_BLOCK);
        block->free = 1;
        if(block->prev != block && block->prev->free){
            block = fusion(block->prev); 
        }
        if(block->next != block && block->next->free){
            block = fusion(block);
        }

        if(block != first_block && block->next == first_block){
            first_block->prev = block->prev;
            block->prev->next = first_block;
            sbrk(-SIZE_BLOCK - block->size);
        }
        if(block == first_block && block->next == first_block){
            sbrk(-SIZE_BLOCK - block->size);
            first_block = NULL;
        }
    }
}

void libfree(void *p){
    pthread_mutex_lock(&malloc_mutex);
    my_free(p);
    pthread_mutex_unlock(&malloc_mutex);
}

void copy_block(s_block *src, s_block *dst){
    size_t *sdata, *ddata;
    size_t i;
    sdata = (size_t *)((char *)src + SIZE_BLOCK);
    ddata = (size_t *)((char *)dst + SIZE_BLOCK);
    for(i = 0; (i * 8) < src->size && (i * 8) < dst->size; ++i){
        ddata[i] = sdata[i];
    }
}

void *my_realloc(void *p, size_t size){
    size_t s;
    s_block *block, *new_block;

    if(!p){
        return my_malloc(size);
    }

    if(valid_addres(p)){
        s = align8(size);
        block = (s_block *)(p - SIZE_BLOCK);
        if(block->size >= s){
            if(block->size - s >= (SIZE_BLOCK + 8)){
                split_block(block, s);
            }  
        }else{
            if(block->next != block && block->next->free && (block->size + SIZE_BLOCK + block->next->size) >= s){
                block = fusion(block);
                if(block->size - s >= (SIZE_BLOCK + 8)){
                    split_block(block, s);
                }
            }else{
                void *newp = my_malloc(s);
                if(!newp){
                    return NULL;
                }
                new_block = (s_block *)(newp - SIZE_BLOCK);
                copy_block(block, new_block);
                my_free(p);
                return newp;
            }
        }
        return p; 
    }
    return NULL;
}

void *librealloc(void *p, size_t size){
    pthread_mutex_lock(&malloc_mutex);
    void *res = my_realloc(p, size);
    pthread_mutex_unlock(&malloc_mutex);
    return res;
}
