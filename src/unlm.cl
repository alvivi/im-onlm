
kernel void test(global char* string) {
	string[0] = 'h';
	string[1] = 'e';
	string[2] = 'l';
	string[3] = 'l';
	string[4] = 'o';
	string[5] = '!';
	string[6] = '\0';
}
