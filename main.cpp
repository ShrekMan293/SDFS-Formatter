#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
using namespace std;

/* Errors
* 1: Invalid Usage
* 2: Failed to seek
* 3: Already formatted
* 4: Disc does not exist
* 5: Kernel does not exist
* 6: Failed to write
*/

constexpr auto ESP_LENGTH_BYTES = 104857600; // 100 MB (MiB)

#pragma pack(1)

typedef struct {
	uint8_t flags;
	uint64_t low_id;
	uint64_t high_id;
	uint32_t low_start_sect;
	uint8_t mid_start_sect;
	uint8_t highest_start_lowest_end;
	uint32_t mid_end_sect;
	uint8_t high_end_sect;
} Directory_Entry;

#pragma pack()

uint64_t roundUp(long double val) {
	uint64_t result = (unsigned)llroundl(val);
	if (val > result) {
		result += 1;
	}

	return result;
}

int main(int argc, char** argv) {
	if (argc != 3) {
		cout << "Usage: <SDFS Formatter> <disc> <kernel_image>\n";
		return 1;
	}

	char* path = argv[1];
	char* kPath = argv[2];

	fstream file(path, ios::binary | ios::in | ios::out);

	if (!filesystem::exists(path)) {
		cout << "File (" << path << ") does not exist.\n";
		return 4;
	}
	if (!filesystem::exists(kPath)) {
		cout << "Kernel image path (" << kPath << ") does not exist.\n";
		return 5;
	}

	char signature[9];

	file.seekg(0, ios::end);
	uint64_t sizeInBytes = file.tellg();
	file.seekg(ESP_LENGTH_BYTES, ios::beg);
	if (file.fail() || file.tellg() != ESP_LENGTH_BYTES) {
		cerr << "Failed to seek past ESP.\n";
		return 2;
	}

	file.read(signature, 8);
	signature[8] = '\0';

	if (strcmp(signature, "SDFSSDFS") == 0) {
		string input;
		cout << "SDFS is already present on this system.\nWould you like to reformat?\nIf so enter 'SDFS': ";
		cin >> input;
		if (input != "SDFS" && input != "sdfs") {
			cout << "Input is not valid. Exiting program.\n";
			return 3;
		}
	}
	
	strcpy(signature, "SDFSSDFS");	// Write signature
	file.seekg(-8, ios::cur);
	file.write(signature, 8);

	strcpy(signature, "\0\0\0\0\0\0\0\0");
	file.write(signature, 8);	// Padding

	uint64_t sizeOfKernel = filesystem::file_size(kPath);
	uint64_t sectorsForKernel = roundUp((long double)sizeOfKernel / 512);

	uint64_t sizeOfFSystem = sizeInBytes / 4096;
	uint64_t sectorsForFSystem = sizeOfFSystem / 512;

	uint32_t lowest32FSystem = static_cast<uint32_t>(sectorsForFSystem);

	uint8_t next8FSystem = static_cast<uint8_t>((sectorsForFSystem >> 32));

	// Extract the highest bit (bit 40) from first64 and set it as the lowest bit (bit 0) of next8
	uint8_t last_first_bit = static_cast<uint8_t>((sectorsForFSystem >> 40) & 0x01);

	// Extract the lowest bit (bit 0) from second64 and set it as the highest bit (bit 7) of next8
	last_first_bit |= static_cast<uint8_t>(((sectorsForKernel + sectorsForFSystem) & 0x01) << 7);

	uint32_t mid32Kernel = static_cast<uint32_t>((sectorsForKernel + sectorsForFSystem) >> 1);
	uint8_t high_8_kernel = static_cast<uint8_t>((sectorsForKernel + sectorsForFSystem) >> 33);

	Directory_Entry yutbooFolder = {
		0b11000000,	// Flags (System, R&W, Shown, Unlocked)
		0x01ULL,	// Low ID
		0x00ULL,	// High ID
		lowest32FSystem,	// Lowest 32 of File System sector
		next8FSystem,		// Next 8
		last_first_bit,		// Highest bit of FS and lowest bit of Kernel sector
		mid32Kernel,		// Next 32 of kernel
		high_8_kernel,		// Highest 8 of kernel
	};

	char serializedData[sizeof(Directory_Entry)];
	memcpy(serializedData, &yutbooFolder, sizeof(Directory_Entry));

	file.write(serializedData, sizeof(Directory_Entry));

	uint64_t offset = sizeOfFSystem - sizeof(Directory_Entry);

	file.seekg(offset, ios::cur);

	ifstream kernel(kPath, ios::in | ios::binary);

	char* kernel_contents = new char[sizeOfKernel];
	kernel.read(kernel_contents, sizeOfKernel);
	file.write(kernel_contents, sizeOfKernel);

	if (file.fail()) {
		cout << "Failure writing kernel.\n";
		return 6;
	}

	/*cout << file.tellg() << '\n';
	cout << sizeInBytes << '\n';
	cout << sizeOfFSystem << '\n';*/

	return 0;
}