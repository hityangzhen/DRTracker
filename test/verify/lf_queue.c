/*
 * Description: lock free queue, for mulit process or thread read and write
 *     History: damonyang@tencent.com, 2013/03/06, create
 */

# include <stdbool.h>
# include <stdlib.h>
# include <string.h>
# include <strings.h>
# include <sys/ipc.h>
# include <sys/shm.h>
# include "lf_queue.h"

#ifndef ATOMIC
#include <pthread.h>
pthread_mutex_t inner_lock=PTHREAD_MUTEX_INITIALIZER;
#endif

# pragma pack(1)

#ifdef ATOMIC
static int32_t bool_compare_and_swap(volatile int32_t* ptr, int32_t oldval, int32_t newval) {
    return __sync_bool_compare_and_swap(ptr,oldval,newval);
}
static int32_t val_compare_and_swap(volatile int32_t* ptr, int32_t oldval, int32_t newval) {
    return __sync_val_compare_and_swap(ptr,oldval,newval);
}
static int32_t fetch_and_add(volatile int32_t* ptr, int32_t val) {
    return __sync_fetch_and_add(ptr,val);
}
static int32_t fetch_and_sub(volatile int32_t* ptr, int32_t val) {
    return __sync_fetch_and_sub(ptr,val);
}
#else
static int bool_compare_and_swap(volatile int32_t* ptr, int oldval, int newval) {
    pthread_mutex_lock(&inner_lock);
    int old_reg_val=*ptr;
    int flag=0;
    if(old_reg_val==oldval) {
        *ptr=newval;
        flag=1;        
    }
    pthread_mutex_unlock(&inner_lock);
    return flag;
}
static int val_compare_and_swap(volatile int32_t *ptr,int oldval,int newval) {
    pthread_mutex_lock(&inner_lock);
    int old_reg_val=*ptr;
    if(old_reg_val==oldval)
        *ptr=newval;
    pthread_mutex_unlock(&inner_lock);
    return old_reg_val;
}
static int32_t fetch_and_add(volatile int32_t* ptr, int32_t val) {
    pthread_mutex_lock(&inner_lock);
    int tmp=*ptr;
    *ptr += val;
    pthread_mutex_unlock(&inner_lock);
    return tmp;
}
static int32_t fetch_and_sub(volatile int32_t* ptr, int32_t val) {
    pthread_mutex_lock(&inner_lock);
    int tmp=*ptr;
    *ptr -= val;
    pthread_mutex_unlock(&inner_lock);
    return tmp;
}
#endif

typedef struct
{
    volatile int32_t next;
    volatile int32_t use_flag;
} unit_head;

typedef struct
{
    int32_t unit_size;
    int32_t max_unit_num;

    volatile int32_t p_head; /* 链表头指针 */
    volatile int32_t p_tail; /* 链表尾指针 */

    volatile int32_t w_tail; /* 下次写开始位置 */

    volatile int32_t w_len; /* 写计数器 */
    volatile int32_t r_len; /* 读计数器 */
} queue_head;

# pragma pack()

# define LIST_END (-1)

# define UNIT_HEAD(queue, offset) ((unit_head *)((queue) + sizeof(queue_head) + \
            ((offset) + 1) * (((queue_head *)(queue))->unit_size + sizeof(unit_head))))
# define UNIT_DATA(queue, offset) ((void *)UNIT_HEAD(queue, offset) + sizeof(unit_head))

static void *__get_shm(key_t key, size_t size, int flag)
{
    int shm_id = shmget(key, size, flag);
    if (shm_id < 0)
        return NULL;

    void *p = shmat(shm_id, NULL, 0);
    if (p == (void *)-1)
        return NULL;

    return p;
}

static int get_shm(key_t key, size_t size, void **addr)
{
    if ((*addr = __get_shm(key, size, 0666)) != NULL)
        return 0;

    if ((*addr = __get_shm(key, size, 0666 | IPC_CREAT)) != NULL)
        return 1;

    return -1;
}

int lf_queue_init(lf_queue *queue, key_t shm_key, int32_t unit_size, int32_t max_unit_num)
{
    if (!queue || !unit_size || !max_unit_num)
        return -2;

    if (unit_size > INT32_MAX - sizeof(unit_head))
        return -3;

    /* 获取内存 */
    int32_t unit_size_real = sizeof(unit_head) + unit_size;
    int32_t max_unit_num_real = max_unit_num + 2;
    size_t  mem_size = sizeof(queue_head) + unit_size_real * max_unit_num_real;

    void *memory = NULL;
    bool old_shm = false;

    if (shm_key)
    {
        int ret = get_shm(shm_key, mem_size, &memory);
        if (ret < 0)
            return -1;
        else if (ret == 0)
            old_shm = true;
    }
    else
    {
        if ((memory = calloc(1, mem_size)) == NULL)
            return -1;
    }

    *queue = memory;

    /* 如果是新内存则进行初始化 */
    if (!old_shm)
    {
        volatile queue_head *q_head = *queue;

        q_head->unit_size = unit_size;
        q_head->max_unit_num = max_unit_num;
        q_head->p_head = LIST_END;
        q_head->p_tail = LIST_END;

        UNIT_HEAD(*queue, LIST_END)->next = LIST_END;
    }

    return 0;
}

int lf_queue_push(lf_queue queue, void *unit)
{
    if (!queue || !unit)
        return -2;
    volatile queue_head * head = queue;
    volatile unit_head * u_head;

    /* 检查队列是否可写 */
    int32_t w_len;
    do
    {
        if ((w_len = head->w_len) >= head->max_unit_num)
            return -1;
    } while (!bool_compare_and_swap(&head->w_len, w_len, w_len + 1));

    /* 为新单元分配内存 */
    int32_t w_tail, old_w_tail;
    do
    {
        do
        {
            old_w_tail = w_tail = head->w_tail;
            w_tail %= (head->max_unit_num + 1);
        } while (!bool_compare_and_swap(&head->w_tail, old_w_tail, w_tail + 1));

        u_head = UNIT_HEAD(queue, w_tail);
    } while (u_head->use_flag);

    /* 写单元头 */
    u_head->next = LIST_END;
    u_head->use_flag = true;

    /* 写数据  */
    memcpy(UNIT_DATA(queue, w_tail), unit, head->unit_size);

    /* 将写完的单元插入链表尾  */
    int32_t p_tail, old_p_tail;
    int try_times = 0;
    do
    {
        old_p_tail = p_tail = head->p_tail;
        u_head = UNIT_HEAD(queue, p_tail);

        if ((++try_times) >= 3)
        {
            while (u_head->next != LIST_END)
            {
                p_tail = u_head->next;
                u_head = UNIT_HEAD(queue, p_tail);
            }
        }
    } while (!bool_compare_and_swap(&u_head->next, LIST_END, w_tail));

    /* 更新链表尾 */
    val_compare_and_swap(&head->p_tail, old_p_tail, w_tail);

    /* 更新读计数器 */
    fetch_and_add(&head->r_len, 1);
    return 0;
}

int lf_queue_pop(lf_queue queue, void *unit)
{
    if (!queue || !unit)
        return -2;
    volatile queue_head *head = queue;
    volatile unit_head *u_head;

    /* 检查队列是否可读 */
    int32_t r_len;
    do
    {
        if ((r_len = head->r_len) <= 0)
            return -1;
    } while (!bool_compare_and_swap(&head->r_len, r_len, r_len - 1));

    /* 从链表头取出一个单元 */
    int32_t p_head;
    do
    {
        p_head = head->p_head;
        u_head = UNIT_HEAD(queue, p_head);
    } while (!bool_compare_and_swap(&head->p_head, p_head, u_head->next));

    /* 读数据 */
    memcpy(unit, UNIT_DATA(queue, u_head->next), head->unit_size);

    /* 更新单元头 */
    UNIT_HEAD(queue, u_head->next)->use_flag = false;

    /* 更新写计数器 */
    fetch_and_sub(&head->w_len, 1);
    return 0;
}

int lf_queue_len(lf_queue queue)
{
    if (!queue)
        return -2;

    return ((queue_head *)queue)->r_len;
}

void lf_queue_free(lf_queue* queue)
{
    if(!*queue)
        return ;
    void *tmp=*queue;
    *queue=NULL;
    free(tmp);
}
