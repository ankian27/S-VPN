#ifndef __CRYPT_H__
#define __CRYPT_H__

struct CodeTable
{
	unsigned char encode[256];
	unsigned char decode[256];
};

void BuildTable(struct CodeTable* ct, const unsigned char* passmd5, const long long timestamp);

void Encrypt(const struct CodeTable* ct, const void* input, void* output, unsigned int len);

void Decrypt(const struct CodeTable* ct, const void* input, void* output, unsigned int len);

#endif