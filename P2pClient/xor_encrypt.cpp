#include"xor_encrypt.h"

// XOR 加密/解密函数
std::string xorEncrypt(const std::string& text, char key)
{
	std::string result = text;

	for (size_t i = 0; i < result.size(); i++)
	{
		result[i] ^= key;
	}

	return result;
}

// XOR 加密/解密函数
std::string xorDecrypt(const std::string& text, char key)
{
	std::string result = text;

	for (size_t i = 0; i < result.size(); i++)
	{
		result[i] ^= key;
	}

	return result;
}