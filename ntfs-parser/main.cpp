#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#pragma pack(push, 1)
struct NTFSBootSector {
    uint8_t jmp[3];
    uint8_t oem[8];
    uint16_t bytesPerSector;
    uint8_t sectorPerCluster;
    uint16_t reservedSectors;
    uint8_t reversed1[5];
    uint8_t media;
    uint8_t reversed2[18];
    uint64_t totalSectors;
    uint8_t startOfMFT[8];
    uint64_t startOfMFTMirr;
    uint8_t MFTEntrySize;
    uint8_t reversed3[3];
    uint8_t indexRecordSize;
    uint64_t serialNumber;
    uint8_t reversed4[430];
    uint8_t signature[2];
};

struct MFTEntryHeader {
    uint32_t signature;
    uint16_t fixupOffset;
    uint16_t fixupCount;
    uint64_t LSN;
    uint16_t sequenceNumber;
    uint16_t linkCount;
    uint8_t attributeOffset[2];
    uint16_t flags;
    uint32_t usedSize;
    uint32_t allocatedSize;
    uint64_t baseFileRecord;
    uint16_t nextAttributeID;
};

struct AttributeHeader {
    uint32_t attributeType;
    uint8_t length[4];
    uint8_t nonResidentFlag;
    uint8_t nameLength;
    uint16_t nameOffset;
    uint16_t flags;
    uint16_t attributeID;
};

struct NonResident {
    uint8_t startVCN[8];
    uint8_t endVCN[8];
    uint8_t runListOffset[2];
    uint16_t compressionUnitSize;
    uint32_t padding;
    uint64_t allocatedSize;
    uint64_t dataSize;
    uint64_t initializedSize;
};
#pragma pack(pop)

void usage() {
    printf("syntas: ntfs-parser <file>\n");
    printf("sample: ntfs-parser a.dd\n");
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

uint64_t convert_to_big_endian(uint8_t* bytes, int size) {
    uint64_t result = 0;
    for (int i = 0; i < size; i++) {
        result |= (uint64_t)bytes[i] << (8 * i);
    }
    return result;
}

void read_runlist(FILE* file, uint32_t offset, uint16_t runlist_offset) {
    uint32_t runlist = offset + runlist_offset;
    uint64_t lcn = 0;

    while(1) {
        // printf("%X\n", runlist);
        fseek(file, runlist, SEEK_SET);
        uint8_t byte;
        fread(&byte, sizeof(uint8_t), 1, file);
        runlist += 1;
        
        if(byte == 0) {
            break;
        } else {
            uint8_t run_offset_size = (byte & 0xF0) >> 4;
            uint8_t run_length_size = byte & 0x0F;
            
            fseek(file, runlist, SEEK_SET);
            uint8_t run_length[8] = {0};  // 최대 8바이트 할당
            fread(run_length, run_length_size, 1, file);
            uint64_t length = convert_to_big_endian(run_length, run_length_size);
            // printf("%lu ", length);
            runlist += run_length_size;

            fseek(file, runlist, SEEK_SET);
            uint8_t run_offset[8] = {0};  // 최대 8바이트 할당
            fread(run_offset, run_offset_size, 1, file);
            int64_t offset = (int64_t)convert_to_big_endian(run_offset, run_offset_size);
            
            // 음수 처리
            if (run_offset_size > 0 && (offset & ((uint64_t)1 << (run_offset_size * 8 - 1)))) {
                offset |= ~0ULL << (run_offset_size * 8);
            }
            // printf("%ld %ld\n", lcn, offset);
            lcn = lcn + offset;
            
            printf("%ld ", lcn);
            printf("%lu\n", length);
            runlist += run_offset_size;
        }
    }
}

// void read_runlist(FILE* file, uint32_t offset, uint16_t runlist_offset) {
//     uint32_t runlist = offset + runlist_offset + 1;
//     while(1) {
//         fseek(file, runlist, SEEK_SET);
//         uint8_t byte;
//         fread(&byte, sizeof(uint8_t), 1, file);
//         // printf("%X ", byte);
//         runlist += 1;
//         if(byte == 0) {
//             break;
//         } else {
//             uint8_t run_offset_size = (byte & 0xF0) >> 4;
//             // printf("Run offset size: %d\n", run_offset_size);
//             uint8_t run_length_size = byte & 0x0F;
//             // printf("Run length size: %d\n", run_length_size);
//             fseek(file, runlist, SEEK_SET);
//             uint8_t run_length[run_length_size];
//             fread(run_length, run_length_size, 1, file);
//             for (int i = 0; i < run_length_size; i++) {
//                 printf("%d", run_length[i]);
//             }
//             printf(" ");
//             runlist += run_length_size;

//             fseek(file, runlist, SEEK_SET);
//             uint8_t run_offset[run_offset_size];
//             fread(run_offset, run_offset_size, 1, file);
//             for (int i = 0; i < run_offset_size; i++) {
//                 printf("%X", run_offset[i]);
//             }
//             printf("\n");
//             runlist += run_offset_size;
//         }
//     }
// }

void read_attribute(FILE* file, uint32_t start_mft, uint16_t attribute_offset) {
    uint32_t offset = start_mft + attribute_offset;
    while (1) {
        // printf("%X\n", offset);
        fseek(file, offset, SEEK_SET);
        struct AttributeHeader* attribute = (struct AttributeHeader*)malloc(sizeof(struct AttributeHeader));
        fread(attribute, sizeof(struct AttributeHeader), 1, file);
        uint32_t attribute_length = ltob_32(attribute->length);
        uint8_t non_resident_flag = attribute->nonResidentFlag;
        // printf("non resident flag: %X, attribute length: %X\n", non_resident_flag, attribute_length);
        if (attribute->attributeType == 0xFFFFFFFF) {
            // printf("No more attributes\n");
            free(attribute);
            break;
        }
        if(non_resident_flag == 1) {
            // printf("Non-resident\n");
            fseek(file, offset + sizeof(struct AttributeHeader), SEEK_SET);
            // printf("Offset: %X\n", offset+sizeof(struct AttributeHeader));
            struct NonResident* non_resident = (struct NonResident*)malloc(sizeof(struct NonResident));
            fread(non_resident, sizeof(struct NonResident), 1, file);
            uint16_t runlist_offset = ltob_16(non_resident->runListOffset);
            // printf("Offset: %X\n", offset);
            // printf("Runlist offset: %X\n", runlist_offset);
            read_runlist(file, offset, runlist_offset);
            offset += attribute_length;
            free(non_resident);
        }
        else {
            // printf("Resident\n");
            offset += attribute_length;
        }
        free(attribute);
    }
}

// void read_attribute(FILE* file, uint32_t start_mft, uint16_t attribute_offset) {
//     uint32_t offset = start_mft + attribute_offset;
//     while (1) {
//         printf("Offset: %X\n", offset);
//         fseek(file, offset, SEEK_SET);
//         struct AttributeHeader attribute;
//         fread(&attribute, sizeof(struct AttributeHeader), 1, file);
        
//         // length 필드를 리틀 엔디안에서 빅 엔디안으로 변환
//         uint32_t attribute_length = ltob_32((uint8_t*)&attribute.length);
        
//         printf("Attribute Type: %X, Length: %X, Non-Resident Flag: %X\n", 
//                attribute.attributeType, attribute_length, attribute.nonResidentFlag);

//         if (attribute.attributeType == 0xFFFFFFFF) {
//             break;
//         }

//         if (attribute.nonResidentFlag) {
//             printf("Non-resident attribute\n");
//             struct NonResident non_resident;
//             fread(&non_resident, sizeof(struct NonResident), 1, file);
//             uint16_t runlist_offset = ltob_16(non_resident.runListOffset);
//             read_runlist(file, offset, runlist_offset);
//         } else {
//             printf("Resident attribute\n");
//         }

//         offset += attribute_length;
//     }
// }

void read_first_mft(FILE* file, uint32_t start_mft) {
    fseek(file, start_mft, SEEK_SET);
    struct MFTEntryHeader* mft = (struct MFTEntryHeader*)malloc(sizeof(struct MFTEntryHeader));
    fread(mft, sizeof(struct MFTEntryHeader), 1, file);
    uint16_t attribute_offset = ltob_16(mft->attributeOffset);

    // printf("%X\n", attribute_offset);
    read_attribute(file, start_mft, attribute_offset);
    free(mft);
}

void read_boot_sector(char* file_name) {
    FILE* file = fopen(file_name, "rb");
	if (!file) {
		fprintf(stderr, "Failed to open %s: %s\n", file_name, strerror(errno));
		exit(EXIT_FAILURE);
	}
    struct NTFSBootSector* bs = (struct NTFSBootSector*)malloc(sizeof(struct NTFSBootSector));
    fread(bs, sizeof(struct NTFSBootSector), 1, file);
    uint16_t sector_size = bs->bytesPerSector;
    uint8_t cluster_size = bs->sectorPerCluster;
    uint64_t start_mft = ltob_64(bs->startOfMFT);
    // printf("%X\n", start_mft * sector_size * cluster_size);
    read_first_mft(file, start_mft * sector_size * cluster_size);

    free(bs);
    fclose(file);    
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        usage();
        exit(EXIT_FAILURE);
    }

    read_boot_sector(argv[1]);

    return 0;
}
