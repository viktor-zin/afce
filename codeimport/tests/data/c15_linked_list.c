#include <stdio.h>
#include <stdlib.h>

struct node {
    int value;
    struct node *next;
};

struct node *push_front(struct node *head, int value)
{
    struct node *n = malloc(sizeof *n);
    if (n == NULL) {
        perror("malloc");
        exit(1);
    }
    n->value = value;
    n->next = head;
    return n;
}

void print_list(const struct node *head)
{
    const struct node *p;
    for (p = head; p != NULL; p = p->next)
        printf("%d -> ", p->value);
    printf("NULL\n");
}

struct node *reverse_list(struct node *head)
{
    struct node *prev = NULL;
    while (head) {
        struct node *next = head->next;
        head->next = prev;
        prev = head;
        head = next;
    }
    return prev;
}

void free_list(struct node *head)
{
    while (head != NULL) {
        struct node *tmp = head;
        head = head->next;
        free(tmp);
    }
}

int main(void)
{
    struct node *list = NULL;
    int x;
    while (scanf("%d", &x) == 1 && x != 0)
        list = push_front(list, x);
    print_list(list);
    list = reverse_list(list);
    print_list(list);
    free_list(list);
    return 0;
}
