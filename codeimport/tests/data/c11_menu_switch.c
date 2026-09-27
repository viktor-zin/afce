#include <stdio.h>

int main(void)
{
    int choice;
    double balance = 0, amount;

    do {
        printf("1 - deposit\n2 - withdraw\n3 - balance\n0 - exit\n> ");
        if (scanf("%d", &choice) != 1)
            break;
        switch (choice) {
        case 1:
            printf("Amount: ");
            scanf("%lf", &amount);
            balance += amount;
            break;
        case 2:
            printf("Amount: ");
            scanf("%lf", &amount);
            if (amount > balance) {
                printf("Not enough money\n");
                break;
            }
            balance -= amount;
            /* fall through: show the balance after a withdrawal */
        case 3:
            printf("Balance: %.2f\n", balance);
            break;
        case 0:
            printf("Bye!\n");
            break;
        default:
            printf("Unknown command %d\n", choice);
        }
    } while (choice != 0);
    return 0;
}
