#include <stdlib.h>
#include <string.h>
#include <stdio.h>


static size_t isqrt(size_t n);
static int is_prime(size_t n);
static size_t next_prime(size_t n);


static size_t isqrt (size_t n) {
	size_t  l = 0, a = 1, d = 3;

	while (a <= n) {
		a += d;
		d += 2;
		l += 1;
	}
	return l;
}

static int is_prime (size_t n) {
	size_t  sqrt, i;

	if (n <= 3) {
		return n > 1;
	}
	if (n % 2 == 0 || n % 3 == 0) {
		return 0;
	}
	sqrt = isqrt(n);
	for (i = 5; i <= sqrt; i += 6) {
		if (n % i == 0 || n % (i + 2) == 0) {
			return 0;
		}
	}
	return 1;
}

static size_t next_prime (size_t n) {
	size_t p;

	p = n;
	while (!is_prime(p)) {
		p++;
	}
	return p;
}

int main () {
	int     n, line_len, prime_len;
	char    buf[128];
	size_t  size, prime, prime_prev;

	n = 0;
	printf("static size_t lws_table_sizes[] = {\n");
	line_len = 0;
	prime_prev = 0;
	for (size = 3; size < ((size_t)1 << 48); size += size / 3) {
		prime = next_prime(size);
		if (prime == prime_prev) {
			continue;
		}
		sprintf(buf, "%zu", prime);
		prime_len = strlen(buf);
		if (line_len == 0) {
			printf("\t%s", buf);
			line_len = 8 + prime_len;
		} else if (line_len + 2 + prime_len < 98) {
			printf(", %s", buf);
			line_len += 2 + prime_len;
		} else {
			printf(",\n\t%s", buf);
			line_len = 8 + prime_len;
		}
		n++;
		prime_prev = prime;
	}
	printf("\n};\n");
	printf("static int lws_table_sizes_n = %d;\n", n);
	return EXIT_SUCCESS;
}
