#define queue_t struct queue_t_t
#define queue_node_t struct queue_node_t_t

#ifndef SIM_QUEUE
#define SIM_QUEUE

queue_t* create_queue();
void destroy_queue(queue_t *q);
void enqueue_item(queue_t *q, void *item);
void* dequeue_item(queue_t *q);
int is_empty (queue_t *q);

struct queue_node_t_t
{
	queue_node_t *next;
	void *item;
};

struct queue_t_t
{
	queue_node_t *head;
	queue_node_t *tail;
	int count;
};

#endif
