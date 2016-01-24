// Copyright 2011 The University of Michigan
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Authors - Jie Yu (jieyu@umich.edu)

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <pthread.h>

typedef struct StringBuffer {
  char *value;
  int value_length;
  int count;
  pthread_mutex_t mutex_lock;
} StringBuffer;

static StringBuffer *null_buffer=NULL;
StringBuffer *buffer=NULL;

static void expand_capacity(StringBuffer *str_buffer,int minimum_capacity)
{
  int new_capacity = (str_buffer->value_length + 1) * 2;
  if (new_capacity < 0) {
    new_capacity = INT_MAX;
  } else if (minimum_capacity > new_capacity) {
    new_capacity = minimum_capacity;
  }
  char *new_value = (char *)malloc(sizeof(char)*new_capacity);
  memcpy(new_value, str_buffer->value, str_buffer->count);
  free(str_buffer->value);
  str_buffer->value = new_value;
  str_buffer->value_length = new_capacity;
}

void append_str(StringBuffer *str_buffer,const char *str)
{
  pthread_mutex_lock(&str_buffer->mutex_lock);
  if(str==NULL)
    str="null";
  int len=strlen(str);
  int new_count=str_buffer->count+len;
  if(new_count>str_buffer->value_length)
    expand_capacity(str_buffer,new_count);
  memcpy(str_buffer->value+str_buffer->count,str,len);
  str_buffer->count=new_count;
  pthread_mutex_unlock(&str_buffer->mutex_lock);
}

int length(StringBuffer *str_buffer)
{
  pthread_mutex_lock(&str_buffer->mutex_lock);
  int ret=str_buffer->count;
  pthread_mutex_unlock(&str_buffer->mutex_lock);
  return ret;
}

void get_chars(StringBuffer *str_buffer,int src_bgn,int src_end,char *dst,
  int dst_bgn)
{
  pthread_mutex_lock(&str_buffer->mutex_lock);
  if(src_bgn<0)
    assert(0);
  if(src_end<0 || src_end>str_buffer->count)
    assert(0);
  if(src_bgn>src_end)
    assert(0);
  memcpy(dst+dst_bgn,str_buffer->value+src_bgn,src_end-src_bgn);
  pthread_mutex_unlock(&str_buffer->mutex_lock);
}

void append_str_buffer(StringBuffer *str_buffer,StringBuffer *sb)
{
  pthread_mutex_lock(&str_buffer->mutex_lock);
  if(sb==NULL)
    sb=null_buffer;
  int len=length(sb);
  int new_count=str_buffer->count+len;
  if(new_count>str_buffer->value_length)
    expand_capacity(str_buffer,new_count);
  get_chars(sb,0,len,str_buffer->value,str_buffer->count);
  str_buffer->count=new_count;
  pthread_mutex_unlock(&str_buffer->mutex_lock);
}

void erase(StringBuffer *str_buffer,int start,int end)
{
  pthread_mutex_lock(&str_buffer->mutex_lock);
  if(start<0)
    assert(0);
  if(end>str_buffer->count)
    end=str_buffer->count;
  if(start>end)
    assert(0);
  int len=end-start;
  if(len>0) {
    memcpy(str_buffer->value+start,str_buffer->value+start+len,
      str_buffer->count-end);

    str_buffer->count -= len;
  }
  pthread_mutex_unlock(&str_buffer->mutex_lock);
}

//constructor
StringBuffer *create_str_buffer()
{
  StringBuffer *str_buffer=(StringBuffer *)malloc(sizeof(*str_buffer));
  str_buffer->value=(char *)malloc(sizeof(char)*16);
  str_buffer->value_length=16;
  str_buffer->count=0;
  pthread_mutex_init(&str_buffer->mutex_lock,NULL);
  return str_buffer;
}

StringBuffer *create_str_buffer_by_len(int len)
{
  StringBuffer *str_buffer=(StringBuffer *)malloc(sizeof(*str_buffer));
  str_buffer->value=(char *)malloc(sizeof(char)*len);
  str_buffer->value_length=len;
  str_buffer->count=0;
  pthread_mutex_init(&str_buffer->mutex_lock,NULL);
  return str_buffer;
}

StringBuffer *create_str_buffer_by_str(const char *str)
{
  StringBuffer *str_buffer=(StringBuffer *)malloc(sizeof(*str_buffer));
  int len=strlen(str)+16;
  str_buffer->value=(char *)malloc(sizeof(char)*len);
  str_buffer->value_length=len;
  str_buffer->count=0;
  pthread_mutex_init(&str_buffer->mutex_lock,NULL);
  append_str(str_buffer,str);
  return str_buffer;
}

void destroy_str_buffer(StringBuffer *str_buffer)
{
  free(str_buffer->value);
  free(str_buffer);
}

void *thread_main(void *args)
{
  printf("erasing the buffer\n");
  erase(buffer,0,3);
  append_str(buffer,"abc");
  printf("erasing done\n");
  return NULL;
}

int main()
{
  null_buffer=create_str_buffer_by_str("null");
  buffer=create_str_buffer_by_str("abc");
  pthread_t tid;
  pthread_create(&tid,NULL,thread_main,NULL);

  StringBuffer *sb=create_str_buffer();
  printf("append the buffer\n");
  append_str_buffer(sb,buffer);
  printf("appending done\n");

  pthread_join(tid,NULL);
  destroy_str_buffer(buffer);
  destroy_str_buffer(null_buffer);
  destroy_str_buffer(sb);
  return 0;
}