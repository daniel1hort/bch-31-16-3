#include"bch31_16_3.h"
#include<assert.h>
#include<stdlib.h>
#include<time.h>

typedef float f32;
#define PRECISION RAND_MAX
#define BUFFER_LEN 1024

u32 bch_encode_string(u8* source, u32 source_len, u8* out, u32 out_len);
u32 bch_decode_string(u8* source, u32 source_len, u8* out, u32 out_len);
u32 bch_data_from_string(u8* source, u32 source_len, u8* out, u32 out_len);

f32 random();
void save_error_correction_distribution(u8* file_name);

int main(void)
{
	bch_init();
	//printAlpha();

	u8 message[] = "Hello world!";
	u32 message_len = strlen(message);
	printf("original data: %s\n", message);

	u8 buffer[256] = { 0 };
	u32 buffer_len = 256;
	u32 encoded_len = bch_encode_string(message, message_len, buffer, buffer_len);

	//add 2 errors in the first word
	buffer[3] ^= ((1 << 0) | (1 << 3));
	//add 3 errors in the second word
	buffer[6] ^= ((1 << 0) | (1 << 4) | (1 << 6));

	u8 buffer2[256] = { 0 };
	u32 buffer2_len = 256;
	u32 decoded_len = bch_data_from_string(buffer, encoded_len, buffer2, buffer2_len);
	printf("received:      %s\n", buffer2);

	memset(buffer2, 0, buffer2_len);
	decoded_len = bch_decode_string(buffer, encoded_len, buffer2, buffer2_len);
	printf("decoded data:  %s\n", buffer2);

	//-----------------------------------------------------------------------------------

	save_error_correction_distribution("results.csv");

	return 0;
}

u32 bch_encode_string(u8* source, u32 source_len, u8* out, u32 out_len) {
	u32 final_len = (source_len + 1) / 2 * 4; //calculate the memory needed to store the output
	assert(final_len <= out_len);

	for (u32 i = 0; i < source_len / 2; i += 1) {
		u16 data = *(u16*)&source[2 * i]; //type punning (https://en.wikipedia.org/wiki/Type_punning)
		u32 word = bch_encode_word(data);
		u32* out_ptr = (u32*)&out[4 * i];
		*out_ptr = word;
	}

	if (source_len % 2 == 1) { //check if last block needs padding
		u16 data = source[source_len - 1];
		u32 word = bch_encode_word(data);
		u32* out_ptr = (u32*)&out[final_len - 4];
		*out_ptr = word;
	}

	return final_len;
}

u32 bch_decode_string(u8* source, u32 source_len, u8* out, u32 out_len) {
	assert(source_len % 4 == 0); //should be multiple of 4 if encoded correcly
	assert(source_len / 2 <= out_len); //output is half the size

	for (u32 i = 0; i < source_len / 4; i += 1) {
		u32 word = *(u32*)&source[4 * i];
		u16 data = bch_decode_word(word);
		u16* out_ptr = (u16*)&out[2 * i];
		*out_ptr = data;
	}

	return source_len / 2;
}

u32 bch_data_from_string(u8* source, u32 source_len, u8* out, u32 out_len) {
	assert(source_len % 4 == 0); //should be multiple of 4 if encoded correcly
	assert(source_len / 2 <= out_len); //output is half the size

	for (u32 i = 0; i < source_len / 4; i += 1) {
		u32 word = *(u32*)&source[4 * i];
		u16 data = bch_data_from_word(word);
		u16* out_ptr = (u16*)&out[2 * i];
		*out_ptr = data;
	}

	return source_len / 2;
}

f32 random() {
	u32 value = rand() % PRECISION;
	f32 percent = (f32)value / PRECISION;
	return percent;
}

void save_error_correction_distribution(u8* file_name) {
	srand(time(NULL)); //seed based on current time
	u16 data[BUFFER_LEN] = { 0 };
	u32 data_encoded[BUFFER_LEN] = { 0 };
	u16 data_decoded[BUFFER_LEN] = { 0 };
	u32 max_it = 100;

	FILE* file = fopen(file_name, "w");
	fprintf(file, "threshold,error_count,error_count_decoded\n");

	for (u32 it = 0; it <= max_it; it += 1) {
		f32 threshold = (f32)it / max_it;
		fprintf(file, "%.3f,", threshold);

		//initialize random data
		for (u32 i = 0; i < BUFFER_LEN; i += 1)
			data[i] = rand();

		//encode data
		bch_encode_string(data, BUFFER_LEN * 2, data_encoded, BUFFER_LEN * 4);
		//flip bits with probability threshold
		for (u32 i = 0; i < BUFFER_LEN; i += 1) {
			for (u32 p = 0; p < 32; p += 1)
				if (random() < threshold)
					data_encoded[i] ^= (1 << p);
		}

		//count error before decoding
		u32 error_count = 0;
		for (u32 i = 0; i < BUFFER_LEN; i += 1) {
			u16 data_from_encoded = bch_data_from_word(data_encoded[i]);
			for (u32 p = 0; p < 32; p += 1)
				if ((data[i] & (1 << p)) != (data_from_encoded & (1 << p)))
					error_count += 1;
		}
		fprintf(file, "%d,", error_count);
		error_count = 0;

		//decode data
		bch_decode_string(data_encoded, BUFFER_LEN * 4, data_decoded, BUFFER_LEN * 2);
		//count errors after decoding
		u32 error_count_decoded = 0;
		for (u32 i = 0; i < BUFFER_LEN; i += 1) {
			for (u32 p = 0; p < 32; p += 1)
				if ((data[i] & (1 << p)) != (data_decoded[i] & (1 << p)))
					error_count_decoded += 1;
		}
		fprintf(file, "%d\n", error_count_decoded);
	}
}
