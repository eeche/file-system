#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#pragma pack(push, 1)
struct FAT32BootSector {
    uint8_t jmp[3];
    uint8_t oem[8];
    uint16_t bytesPerSector;
    uint8_t sectorPerCluster;
    uint16_t reservedSectorCount;
    uint8_t numberOfFATs;
    uint16_t rootDirEntryCount;
    uint16_t totalSector16;
    uint8_t media;
    uint16_t FATSize16;
    uint16_t sectorPerTrack;
    uint16_t numberOfHeads;
    uint32_t hiddenSector;
    uint32_t totalSector32;
    uint32_t FATSize32;
    uint16_t exitFlags;
    uint16_t fileSystemVersion;
    uint32_t rootDirCluster;
    uint16_t fileSystemInfo;
    uint16_t rootRecordBackupSec;
    uint8_t reserved[12];
    uint8_t driveNumber;
    uint8_t reserved1;
    uint8_t bootSignature;
    uint32_t volumeID;
    uint8_t volumeLabel[11];
    uint8_t fileSystemType[8];
};
#pragma pack(pop)

void usage() {
    printf("syntas: fat32-parser <file> <starting cluster>\n");
    printf("sample: fat32-parser a.dd 1\n");
}

uint32_t ltob_32(uint8_t* bytes) {
	return bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
}

void read_fat_chain(FILE* file, uint32_t start_sector, uint32_t reserved_size, uint32_t bytes_per_sector, uint32_t sector_per_cluster) {
    uint32_t current_sector = start_sector;
    uint32_t fat_offset, sector_number;
    uint8_t buffer[4];

    while (current_sector != 0xFFFFFFF) {
        printf("%d ", current_sector);
        fat_offset = current_sector * 4;
        sector_number = reserved_size * bytes_per_sector + fat_offset;

        fseek(file, sector_number, SEEK_SET);
        fread(buffer, sizeof(buffer), 1, file);
        current_sector = ltob_32(buffer);
    }
    printf("\n");
}

void read_boot_sector(const char* file_name, int start_sector) {
    FILE* file = fopen(file_name, "rb");
    if (file == NULL) {
        perror("fopen");
        exit(1);
    }
    struct FAT32BootSector* bs = (struct FAT32BootSector*)malloc(sizeof(struct FAT32BootSector));
    fread(bs, sizeof(struct FAT32BootSector), 1, file);
    uint32_t bytes_per_sector = bs->bytesPerSector;
    uint32_t sector_per_cluster = bs->sectorPerCluster;
    uint32_t reserved_size = bs->reservedSectorCount;
    uint32_t fat_size = bs->FATSize32;
    read_fat_chain(file, start_sector, reserved_size, bytes_per_sector, sector_per_cluster);

    free(bs);
    fclose(file);
}

int main(int argc, char* argv[]) {

    if (argc != 3) {
        usage();
        exit(EXIT_FAILURE);
    }

    const char* file = argv[1];
    char* endptr;
    
    // 문자열을 정수로 변환합니다.
    int start_sector = strtol(argv[2], &endptr, 10);
    
    // 변환 오류 검사
    if (*endptr != '\0') {
        fprintf(stderr, "Invalid starting_cluster value: %s\n", argv[2]);
        return 1;
    }
    printf("%d ", start_sector);
    read_boot_sector(file, start_sector);

    return 0;
}