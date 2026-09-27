#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define MAXLEN 256

int my_strlen(const char *s)
{
    int n = 0;
    while (s[n] != '\0')
        n++;
    return n;
}

void reverse(char *s)
{
    int i = 0, j = my_strlen(s) - 1;
    while (i < j) {
        char c = s[i];
        s[i++] = s[j];
        s[j--] = c;
    }
}

int is_palindrome(const char *s)
{
    int i = 0, j = (int)strlen(s) - 1;
    while (i < j) {
        if (!isalnum((unsigned char)s[i])) { i++; continue; }
        if (!isalnum((unsigned char)s[j])) { j--; continue; }
        if (tolower((unsigned char)s[i]) != tolower((unsigned char)s[j]))
            return 0;
        i++;
        j--;
    }
    return 1;
}

int count_words(const char *s)
{
    int words = 0, in_word = 0;
    for (; *s; s++) {
        if (isspace((unsigned char)*s)) {
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            words++;
        }
    }
    return words;
}

int main(void)
{
    char line[MAXLEN];
    printf("Enter a line: ");
    if (fgets(line, MAXLEN, stdin) == NULL)
        return 1;
    line[strcspn(line, "\n")] = '\0';
    printf("Length: %d, words: %d\n", my_strlen(line), count_words(line));
    printf(is_palindrome(line) ? "Palindrome\n" : "Not a palindrome\n");
    reverse(line);
    printf("Reversed: \"%s\"\n", line);
    for (char *p = line; *p; p++)
        *p = toupper((unsigned char)*p);
    puts(line);
    return 0;
}
