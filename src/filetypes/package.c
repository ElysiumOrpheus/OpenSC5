#include "filetypes/package.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct PackageHeader {
    char magic[4];              //00
    uint32_t majorVersion;      //04
    uint32_t minorVersion;      //08
    uint32_t unknown[3];        //0C
    uint32_t dateCreated;       //18
    uint32_t dateModified;      //1C
    uint32_t indexMajorVersion; //20
    uint32_t indexEntryCount;   //24
    uint32_t firstIndexEntryOffset; //28
    uint32_t indexSize;         //2C
    uint32_t holeEntryCount;    //30
    uint32_t holeOffset;        //34
    uint32_t holeSize;          //38
    uint32_t indexMinorVersion; //3C
    uint32_t indexOffset;       //40
    uint32_t unknown2;          //44
    unsigned char reserved[24]; //48
                                //5C
} PackageHeader;

typedef struct IndexEntry {
    uint32_t type;
    uint32_t group;
    uint32_t instance;
    uint32_t chunkOffset;
    uint32_t diskSize;
    uint32_t memSize;
    uint16_t compressed;
    uint16_t unknown;
    
    bool isCompressed;
} IndexEntry;

typedef struct IndexData {
    uint32_t null;
} IndexData;

typedef struct Index {
    uint32_t indexType;
    
    IndexData data;
} Index;

static void readuint(uint32_t *ret, FILE *f)
{
    fread(ret, sizeof(uint32_t), 1, f);

    if (feof(f))
    {
        printf("Unexpected end of file: %lu.\n", ftell(f));
        exit(EXIT_FAILURE);
    }
}

static uint32_t little2big(uint32_t le)
{
    return __builtin_bswap32(le);
}

static void ProcessPackageData(unsigned char *data, int dataSize, uint32_t dataType)
{
    switch (dataType)
    {
        case 0x00B1B104: // Properties files https://simswiki.info/wiki.php?title=Spore:00B1B104
        {
            uint32_t variableCount = little2big(*(uint32_t*)data);

            data += sizeof(uint32_t);

            printf("Properties Info:\n");
            printf("Variable count: %d\n", variableCount);
            
            for (int i = 0; i < variableCount; i++)
            {
                printf("\nVariable %d:\n", i);

                uint32_t identifier = little2big(*(uint32_t*)data);
                data += sizeof(uint32_t);
                uint16_t type = little2big(*(uint16_t*)data);
                data += sizeof(uint16_t);
                uint16_t specifier = little2big(*(uint16_t*)data);
                data += sizeof(uint16_t);

                printf("Identifier: %#x\n", identifier);
                printf("Type: %#x\n", type);
                printf("Specifier: %#x\n", specifier);
            }
        } break;
        default:
        {
            printf("Unknown data type %#X.\n", dataType);
        } break;
    }
}

void LoadPackageFile(FILE *f)
{
    PackageHeader header;
    fread(&header, sizeof(PackageHeader), 1, f);

    printf("Header:\n");
    printf("Magic: %.4s\n", header.magic);
    printf("Major Version #: %d\n", header.majorVersion);
    printf("Minor Version #: %d\n", header.minorVersion);
    printf("Index Entry Count: %d\n", header.indexEntryCount);
    printf("Index Size: %d\n", header.indexSize);
    printf("Index Major Version: %d\n", header.indexMajorVersion);
    printf("Index Minor Version: %d\n", header.indexMinorVersion);
    printf("Index Offset: %d\n", header.indexOffset);

    fseek(f, header.indexOffset, SEEK_SET);
    Index index = { 0 };

    readuint(&index.indexType, f);

    printf("\nIndex information:\n");
    printf("Index Type: %d\n", index.indexType);

    IndexData indexData = index.data;

    //readuint(&indexData.null, f);

    uint32_t indexTypeId = 0xCAFEBABE;
    if ((index.indexType & (1 << 0)) == 1 << 0)
    {
        readuint(&indexTypeId, f);
    }

    uint32_t indexGroupContainer = 0xCAFEBABE;
    if ((index.indexType & (1 << 1)) == 1 << 1)
    {
        readuint(&indexGroupContainer, f);
    }

    uint32_t indexUnknown = 0xCAFEBABE;
    if ((index.indexType & (1 << 2)) == 1 << 2)
    {
        readuint(&indexUnknown, f);
    }

    IndexEntry *entries = malloc(sizeof(IndexEntry) * header.indexEntryCount);

    for (int i = 0; i < header.indexEntryCount; i++)
    {
        IndexEntry entry = { 0 };

        if ((index.indexType & (1 << 0)) == 1 << 0)
        {
            entry.type = indexTypeId;
        }
        else
        {
            readuint(&entry.type, f);
        }

        if ((index.indexType & (1 << 1)) == 1 << 1)
        {
            entry.group = indexGroupContainer;
        }
        else
        {
            readuint(&entry.group, f);
        }

        if ((index.indexType & (1 << 2)) == 1 << 2)
        {
            uint32_t unk = indexUnknown;
        }
        else
        {
            uint32_t unk;
            readuint(&unk, f);
        }

        readuint(&entry.instance, f);

        readuint(&entry.chunkOffset, f);

        readuint(&entry.diskSize, f);
        entry.diskSize &= ~0x80000000;
        readuint(&entry.memSize, f);
        fread(&entry.compressed, sizeof(uint16_t), 1, f);
        fread(&entry.unknown, sizeof(uint16_t), 1, f);

        entry.isCompressed = (entry.compressed == 0xFFFF);

        printf("\nEntry %d:\n", i);
        printf("Type: %#X\n", entry.type);
        printf("Group: %u\n", entry.group);
        printf("Instance: %u\n", entry.instance);
        printf("Chunk Offset: %u\n", entry.chunkOffset);
        printf("Disk Size: %u\n", entry.diskSize);
        printf("Mem Size: %u\n", entry.memSize);
        printf("Compressed? %s\n", entry.isCompressed?"yes":"no");

        entries[i] = entry;
    }

    printf("\nData Cycle.\n");

    for (int i = 0; i < header.indexEntryCount; i++)
    {
        IndexEntry entry = entries[i];

        printf("\nEntry %d:\n", i);

        if (fseek(f, entry.chunkOffset, SEEK_SET) == -1)
        {
            perror("Unexpected error occurred");
        }

        unsigned char *data = malloc(entry.diskSize);

        fread(data, 1, entry.diskSize, f);

        if (feof(f))
        {
            printf("Unexpected end of file.\n");
        }

        if (!entry.isCompressed)
        {
            ProcessPackageData(data, entry.diskSize, entry.type);
        }
        
        for (int i = 0; i < entry.diskSize; i++)
        {
            printf("%#x ", data[i]);
        }
        puts("");

        free(data);
    }
}
