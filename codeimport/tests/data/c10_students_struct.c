#include <stdio.h>
#include <string.h>

#define MAX_STUDENTS 30

typedef struct {
    char name[32];
    int grades[5];
    double average;
} Student;

struct Group {
    Student list[MAX_STUDENTS];
    int count;
};

double average(const int *g, int n)
{
    int sum = 0;
    for (int i = 0; i < n; i++)
        sum += g[i];
    return (double)sum / n;
}

int best_student(const struct Group *grp)
{
    int best = 0;
    for (int i = 1; i < grp->count; i++)
        if (grp->list[i].average > grp->list[best].average)
            best = i;
    return best;
}

int main(void)
{
    struct Group group;
    group.count = 0;
    printf("How many students? ");
    scanf("%d", &group.count);
    if (group.count > MAX_STUDENTS)
        group.count = MAX_STUDENTS;
    for (int i = 0; i < group.count; i++) {
        Student *s = &group.list[i];
        printf("Name: ");
        scanf("%31s", s->name);
        for (int j = 0; j < 5; j++)
            scanf("%d", &s->grades[j]);
        s->average = average(s->grades, 5);
    }
    if (group.count > 0) {
        int b = best_student(&group);
        printf("Best: %s (%.2f)\n", group.list[b].name, group.list[b].average);
    }
    return 0;
}
