/* A link of an intrusive circular doubly linked list (the kernel's lists of processes, threads, timers, ...). */

#pragma once

typedef struct ListNode {
	struct ListNode *next;
	struct ListNode *prev;
} ListNode;
