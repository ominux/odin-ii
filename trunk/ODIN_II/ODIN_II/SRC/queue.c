/*
Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "queue.h"
#include "types.h"

/*
 * Creates a queue_t struct and instantiates tail. The struct
 * behaves like a linked list, enqueuing at the head and dequeuing
 * at the tail.
 */
queue_t* create_queue()
{
	queue_t *q = (queue_t *)malloc(sizeof(queue_t));
	q->head = NULL;
	q->tail = NULL;
	q->count = 0;

	return q;
}

/*
 * Enqueues an item onto the queue
 */
void enqueue_item(queue_t *q, void *item)
{
	queue_node_t *node;
	
//	node = q->head;
//	while (node != NULL)
//	{
//		if (node->item == item)
//			return;
//		node = node->next;
//	}
	oassert(item != NULL);
	oassert(q != NULL);
	node = (queue_node_t *)malloc(sizeof(queue_node_t));
	node->next = NULL;
	node->item = item;

	q->count++;

	/* add the new node to the end */
	if (q->tail == NULL)
		q->tail = node;
	else
		q->tail->next = node;
	q->tail = node;

	if (q->head == NULL)
		q->head = node;
}

/*
 * dequeues an item from the queue
 */
void* dequeue_item(queue_t *q)
{
	void *item;
	queue_node_t *node;

	oassert(q != NULL);
	oassert(q->head != NULL);

	q->count--;

	node = q->head;
	item = node->item;
	q->head = q->head->next;
	if (q->head == NULL)
		q->tail = NULL;

	free(node);

	return item;
}

/*
 * frees any memory used by the queue, including at nodes left
 * in the queue. It DOES NOT free memory of the things encapsulated
 * in the queue.
 */
void destroy_queue(queue_t *q)
{
	while (!is_empty(q))
		dequeue_item(q);
	free(q);
}

/*
 * returns true iff the queue has no remaining items to be dequeued.
 */
int is_empty (queue_t *q)
{
	return q->head == NULL;
}
