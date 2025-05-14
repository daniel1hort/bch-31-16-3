#pragma once

#include<stdint.h>
#include<stdio.h>
#include<string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

//X^15 + X^11 + X^10 + X^9 + X^8 + X^7 + X^5 + X^3 + X^2 + X + 1
u16 g3 = (1 << 15) | (1 << 11) | (1 << 10) | (1 << 9) |
		 (1 << 8) | (1 << 7) | (1 << 5) | (1 << 3) |
		 (1 << 2) | (1 << 1) | (1 << 0);

u32 len = 32;
u32 alpha[32];
u32 reverse_alpha[32];

void printAlpha(void) {
	for (u32 i = 0; i < len; i += 1) {
		printf("a^%2d = ", i);

		u32 value = alpha[i];
		u32 power = 0;
		u32 is_first = 1;
		while (value != 0) {
			if (value & 1 != 0) {
				if (!is_first)
					printf("+ ");
				is_first = 0;

				if (power == 0)
					printf("1 ");
				else if (power == 1)
					printf("a ");
				else
					printf("a^%d ", power);
			}

			value = value >> 1;
			power += 1;
		}
		printf("\n");
	}
	printf("\n");
}

void printBinary(u32 value) {
	u8 index = 32;

	do {
		index -= 1;
		u32 mask = value & (1 << index);
		u32 result = 0;
		if (mask != 0)
			result = 1;
		printf("%d", result);
		if (index % 8 == 0)
			printf(" ");
	} while (index > 0);

	printf("\n");
}

void calculateAlpha(u32 order, u32 polynom) {
	u32 value = 1;
	u32 it = 0;
	u32 n = 1 << order;

	while (it < n && it < len - 1) {
		alpha[it] = value;
		value = value << 1;
		if ((value & n) != 0) {
			value = value ^ polynom;
		}
		it += 1;
	}
}

//call a bit encoded polynom with the value alpha^power
u32 call_with_alpha(u32 g, u32 power) {
	u32 sum = 0;
	for (u32 i = 0; i < len - 1; i += 1) {
		if ((g & (1 << i)) == 0) continue;

		u32 index = i * power % (len - 1);
		u32 value = alpha[index];
		sum ^= value;
	}
	return sum;
}

// call an array encoded polynom with the value alpha^power
u32 call_with_alpha_array(u32* g, u32 g_len, u32 power) {
	u32 sum = 0;
	for (u32 i = 0; i < g_len; i += 1) {
		if (g[i] == 0) continue;

		u32 index = (i * power + reverse_alpha[g[i]]) % (len - 1);
		u32 value = alpha[index];
		sum ^= value;
	}
	return sum;
}

u32 berlekamp_massey(u32* s, u32 s_len, u32* buffer) {
	u32* C = buffer;
	u32* B = C + s_len;
	u32* T = B + s_len;

	C[0] = 1;
	B[0] = 1;
	u32 L = 0;
	u32 m = 1;
	u32 b = 1;

	for (u32 n = 0; n < s_len; n += 1) {
		u32 d = s[n];
		for (u32 i = 1; i <= L; i += 1) {
			if (n >= i) {
				u32 index = (reverse_alpha[C[i]] + reverse_alpha[s[n - i]]) % (len - 1);
				d ^= alpha[index];
			}
		}

		if (d == 0) {
			m += 1;
		}
		else if (2 * L <= n) {
			memcpy(T, C, s_len * sizeof(u32));

			u32 coef = (reverse_alpha[d] + (len - 1) - reverse_alpha[b]) % (len - 1);
			for (u32 i = m; i <= s_len; i += 1) {
				if (B[i - m] == 0) continue;

				u32 index = (reverse_alpha[B[i - m]] + coef) % (len - 1);
				C[i] ^= alpha[index];
			}

			L = n + 1 - L;
			memcpy(B, T, s_len * sizeof(u32));
			b = d;
			m = 1;
		}
		else {
			u32 coef = (reverse_alpha[d] + (len - 1) - reverse_alpha[b]) % (len - 1);
			for (u32 i = m; i <= s_len; i += 1) {
				if (B[i - m] == 0) continue;

				u32 index = (reverse_alpha[B[i - m]] + coef) % (len - 1);
				C[i] ^= alpha[index];
			}
			m += 1;
		}
	}

	memset(B, 0, s_len * sizeof(u32));
	memset(T, 0, s_len * sizeof(u32));
	return L;
}

void bch_init(void) {
	u32 order = 5;
	u32 polynom = (1 << 5) | (1 << 2) | (1 << 0);
	u32 len = (1 << 5);

	calculateAlpha(order, polynom);

	for (u32 i = 0; i < len; i++) {
		reverse_alpha[alpha[i]] = i;
	}
}

u32 bch_encode_word(u16 data) {
	u32 result = data << 15;
	u32 polynom = g3 << 15;

	for (u8 i = 30; i >= 15; i -= 1) {
		if ((result & (1 << i)) != 0) {
			result ^= polynom;
		}
		polynom >>= 1;
	}

	u32 word = (data << 15) + result;
	return word;
}

void bch_syndrome_word(u32 word, u32* syndrome, u32 s_len) {
	for (u32 i = 1; i <= s_len; i += 1) {
		u32 value = call_with_alpha(word, i);
		syndrome[i - 1] = value;
	}
}

u16 bch_data_from_word(u32 word) {
	return word >> 15;
}

u16 bch_decode_word(u32 word) {
	u32 syndrome[6] = { 0 };
	bch_syndrome_word(word, syndrome, 6);

	u32 buffer[3][6] = { 0 };
	u32 v = berlekamp_massey(syndrome, 6, buffer);

	//brute force search for zeroes
	for (u32 i = 0; i < len - 1; i += 1) {
		u32 value = call_with_alpha_array(buffer[0], v + 1, i);
		if (value == 0)
			word = word ^ (1 << (len - 1 - i));
	}

	return bch_data_from_word(word);
}
