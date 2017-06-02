#define ASSERT_REQUIRE_FAILURE_HOOK     abort()

#include "test-harness.h"

int
main(void)
{
	const char teststring[] = "asdfjkl;";
	uint16_t crc = 0;
	int i;

	for (i = 0; i < strlen(teststring); i++) {
		crc = soot_crc16(crc, teststring[i]);
	}
	printf("sizeof(soot_instance_t) = %d\n",(int)sizeof(soot_instance_t));

	return crc == 0x88F4
		? EXIT_SUCCESS
		: EXIT_FAILURE;
}
