#include "regex.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

int regcomp(regex_t *preg, const char *regex, int cflags)
{
    if (!preg || !regex) {
        return 1;
    }
    preg->cflags = cflags;
    preg->pat = strdup(regex);
    return preg->pat ? 0 : 1;
}

static int icmp_eq(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
}

int regexec(const regex_t *preg, const char *string, size_t nmatch, regmatch_t pmatch[], int eflags)
{
    const char *pat;
    size_t plen;
    int icase;

    (void)nmatch;
    (void)pmatch;
    (void)eflags;

    if (!preg || !preg->pat || !string) {
        return 1;
    }

    pat = preg->pat;
    icase = preg->cflags & REG_ICASE;
    plen = strlen(pat);

    if (pat[0] == '\0' || (plen == 2 && pat[0] == '.' && pat[1] == '*')) {
        return 0;
    }

    if (plen >= 2 && pat[plen - 2] == '.' && pat[plen - 1] == '*') {
        char prefix[128];
        size_t n = plen - 2;
        if (n >= sizeof(prefix)) {
            n = sizeof(prefix) - 1;
        }
        memcpy(prefix, pat, n);
        prefix[n] = '\0';
        if (icase) {
            return strncasecmp(string, prefix, n) == 0 ? 0 : 1;
        }
        return strncmp(string, prefix, n) == 0 ? 0 : 1;
    }

    if (icase) {
        return icmp_eq(pat, string) || strcasestr(string, pat) ? 0 : 1;
    }
    return strcmp(pat, string) == 0 || strstr(string, pat) ? 0 : 1;
}

size_t regerror(int errcode, const regex_t *preg, char *errbuf, size_t errbuf_size)
{
    const char *msg = "regex error";
    (void)errcode;
    (void)preg;
    if (errbuf && errbuf_size) {
        strncpy(errbuf, msg, errbuf_size - 1);
        errbuf[errbuf_size - 1] = '\0';
    }
    return strlen(msg) + 1;
}

void regfree(regex_t *preg)
{
    if (!preg) {
        return;
    }
    free(preg->pat);
    preg->pat = NULL;
}
