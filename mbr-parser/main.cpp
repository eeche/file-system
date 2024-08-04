#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

#define SECTOR 512
#define BOOT_CODE 446

struct PartitionTableEntry {
	uint8_t bootingFlag;
	uint8_t CHSAddress[3];
	uint8_t PartitionType;
	uint8_t CHSAddress2[3];
	uint8_t LBAAddressofStart[4];
	uint8_t PartitionSize[4];
};

void usage() {
	printf("syntas: mbr-parser <file>\n");
	printf("sample: mbr-parser a.dd\n");
}

uint32_t ltob(uint8_t* bytes) {
	return bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
}

void ebr_parser(FILE* file, uint32_t startLBA, uint32_t baseLBA) {
	fseek(file, startLBA * SECTOR + BOOT_CODE, SEEK_SET);

	struct PartitionTableEntry partitions[2];
	size_t result = fread(&partitions, sizeof(struct PartitionTableEntry)*2, 1, file);
	if (!result) {
		fprintf(stderr, "Failed to read EBR partition table entries: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	for (int i = 0; i < 2; i++) {
		struct PartitionTableEntry* entry = &partitions[i];
		if (entry->PartitionType == 0x00) break;
		else if (entry->PartitionType == 0x05) {
			uint32_t ebr_startLBA = ltob(entry->LBAAddressofStart) + baseLBA;
			ebr_parser(file, ebr_startLBA, baseLBA);
		}
		else {
			uint32_t start_point = ltob(entry->LBAAddressofStart) + startLBA;
			uint32_t partition_size = ltob(entry->PartitionSize);
			if (entry->PartitionType == 0x07) printf("NTFS ");
			else if (entry->PartitionType == 0x0b) printf("FAT32 ");
			printf("%d ", start_point);
			printf("%d\n", partition_size);
		}
	}
}

void mbr_parser(char* fileName) {
	FILE* file = fopen(fileName, "rb");
	if (!file) {
		fprintf(stderr, "Failed to open %s: %s\n", fileName, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (fseek(file, BOOT_CODE, SEEK_SET) != 0) {
		fprintf(stderr, "Failed to seek to position %d: %s\n", BOOT_CODE, strerror(errno));
		fclose(file);
		exit(EXIT_FAILURE);
	}

	struct PartitionTableEntry partitions[4];
	size_t result = fread(&partitions, sizeof(struct PartitionTableEntry) * 4, 1, file);
	if (!result) {
		fprintf(stderr, "Failed to read partition table entries: %s\n", strerror(errno));
		fclose(file);
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i < 4; i++) {
		struct PartitionTableEntry* entry = &partitions[i];
		if (entry->PartitionType == 0) break;
		else if (entry->PartitionType == 0x05) {
			uint32_t ebr_startLBA = ltob(entry->LBAAddressofStart);
			ebr_parser(file, ebr_startLBA, ebr_startLBA);
		}
		else {
			uint32_t start_point = ltob(entry->LBAAddressofStart);
			uint32_t partition_size = ltob(entry->PartitionSize);

			if (entry->PartitionType == 0x07) printf("NTFS ");
			else if (entry->PartitionType == 0x0b) printf("FAT32 ");
			printf("%d ", start_point);
			printf("%d\n", partition_size);
		}
	}

	fclose(file);
}

int main(int argc, char* argv[]) {
	
	if (argc != 2) {
		usage();
		exit(EXIT_FAILURE);
	}

	mbr_parser(argv[1]);
	return 0;
}
