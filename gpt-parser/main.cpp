#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

#define SECTOR 512

struct GPTHeader {
    uint8_t signature[8];
    uint8_t revision[4];
    uint8_t headerSize[4];
    uint8_t CRC32ofHeader[4];
    uint8_t reserved[4];
    uint8_t currentLBA[8];
    uint8_t backupLBA[8];
    uint8_t firstUsableLBA[8];
    uint8_t lastUsableLBA[8];
    uint8_t diskGUID[16];
    uint8_t partitionEntriesStartingLBA[8];
    uint8_t numberofPartitionEntries[4];
    uint8_t sizeofPartitionEntry[4];
    uint8_t CRC32ofPartitionArray[4];
};

struct PartitionTableEntry {
    uint8_t t_guid_lfield[4];
    uint8_t t_guid_mfield[2];
    uint8_t t_guid_hfield_version[2];
    uint8_t t_guid_big[8];
    uint8_t u_guid[16];
	uint8_t firstLBA[8];
	uint8_t lastLBA[8];
	uint8_t attributeFlags[8];
	uint8_t partitionName[72];
};

void usage() {
	printf("syntas: gpt-parser <file>\n");
	printf("sample: mbr-parser a.dd\n");
}

uint16_t ltob_16(uint8_t* bytes) {
    return bytes[0] | (bytes[1] << 8);
}

uint32_t ltob_32(uint8_t* bytes) {
	return bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
}

uint64_t ltob_64(uint8_t* bytes) {
    uint64_t result = 0;
    for (int i = 0; i < 8; i++) {
        result |= (uint64_t)bytes[i] << (i * 8);
    }
    return result;
}

int is_empty_partition(struct PartitionTableEntry* entry) {
    static const char zeros[sizeof(struct PartitionTableEntry)] = {0};
    return memcmp(entry, zeros, sizeof(struct PartitionTableEntry)) == 0;
}

void get_GUID(struct PartitionTableEntry* guid) {
    // check if the GUID is valid
    printf("%08X%04X%04X", 
           ltob_32((uint8_t*)&guid->t_guid_lfield),
           ltob_16((uint8_t*)&guid->t_guid_mfield),
           ltob_16((uint8_t*)&guid->t_guid_hfield_version));
    for (int i = 0; i < 8; i++) {
        printf("%02X", guid->t_guid_big[i]);
    }
}

void get_partition_type(FILE* file, uint64_t start_point) {
    fseek(file, start_point * SECTOR, SEEK_SET);
    uint8_t VBR[3];
    fread(VBR, 3, 1, file);
    if (VBR[0] == 0xEB && VBR[1] == 0x52 && VBR[2] == 0x90) {
        printf("NTFS ");
    }
    else if (VBR[0] == 0xEB && VBR[1] == 0x58 && VBR[2] == 0x90) {
        printf("FAT32 ");
    }
}

void gpt_parser(FILE* file, uint32_t entry_num) {
    fseek(file, SECTOR * 2, SEEK_SET);
    struct PartitionTableEntry* entries = (PartitionTableEntry*)malloc(sizeof(PartitionTableEntry) * entry_num);
    fread(entries, sizeof(struct PartitionTableEntry), entry_num, file);
    for (int i = 0; i < entry_num; i++) {
        struct PartitionTableEntry* entry = &entries[i];
        if (is_empty_partition(entry)) {
            break;
        }
        uint64_t start_point = ltob_64(entry->firstLBA);
        uint64_t partition_size = ltob_64(entry->lastLBA) - start_point + 1;
        get_GUID((struct PartitionTableEntry*)&entry->t_guid_lfield);
        printf(" ");
        get_partition_type(file, start_point);
        printf("%llu %llu\n", start_point, partition_size);
    }
    free(entries);
}

void read_partition_table(char* file_name) {
    FILE* file = fopen(file_name, "rb");
    fseek(file, SECTOR, SEEK_SET);
    
    struct GPTHeader* header = (struct GPTHeader*)malloc(sizeof(struct GPTHeader));
    fread(header, sizeof(struct GPTHeader), 1, file);
    uint32_t entry_num = ltob_32(header->numberofPartitionEntries);
    
    gpt_parser(file, entry_num);
    
    free(header);
    fclose(file);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        usage();
        exit(EXIT_FAILURE);
    }
    read_partition_table(argv[1]);
    return 0;
}