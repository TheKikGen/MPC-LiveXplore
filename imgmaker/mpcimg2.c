#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <lzma.h>
#include <openssl/sha.h>


// Functions Prototypes
int DisplayAkaiImageInfo(const char* filename, uint8_t tempFile);
int ExtractRootfsPartition(const char *inImgFilename, const char*outPartFilename );

/* 

Structure analysis

41 5A 30 31 AZ01

01 00 00 00 
74 00 00 00 

17 00 00 00 
53 4E 41 50 53 48 4F 54 2D 32 30 32 35 30 32 32 31 31 32 30 32 35 39 00 

01 00 00 00 /  08 00 00 00    Number of occurences

0C 00 00 00 Len
69 6E 6D 75 73 69 63 2C 61 64 61 32  inmusic,ada2

00 00 00 00 Padding until 16 bytes

0D 00 00 00 inmusic,acva2
00 00 00 padding until 1- bytes

0D 00 00 00 inmusic,acva2
00 00 00 padding until 1- bytes



01 00 00 00 // Device Id nb of occurences
40 40 E8 09 // Device id 4 bytes

25 00 00 00 Igame description len
41 6B 61 69 20 50 72 6F 66 65 73 73 69 6F 6E 61 6C 20 46 6F 72 63 65 20 75 70 67 72 61 64 65 20 69 6D 61 67 65 Akai Professional Force upgrade image
00 00 00 00 00 00 00 Padding to 44 bytes

*/


// Structures

typedef struct __attribute__((packed)) {
    char        header[4]; // AZ01
    uint32_t    Value1;   // 01 00 00 00 (litlle Endian)
    uint32_t    Value2;   // 74 00 00 00 (Force) or 08 01 00 00 
    uint32_t    imgNamelen;   // including 0 terminated.
    char        imgName[24]; // At this stage, it is a hard coded size. Null teminated.
} AkaiImageHeader_t;

typedef struct __attribute__((packed)) {
    uint32_t deviceNameLen;
    char     deviceName[16];
    uint32_t usbId;
} CompatTable_t;


typedef struct  __attribute__((packed)) {
    char     tag[8];        // "PARTL" + padding
    uint64_t size;          // Partiton len 
    uint32_t type;          // Partition type
    char     name[8];       // Partition name
    uint32_t format;        // Partition format
    char     compType[4];   // Compression type
    uint32_t info1;
    uint32_t info2;
    char     hashAlgo[8];   // Hashcode algo
    uint32_t hashLen;       // Hash len
    char     hashCode[20];  // Hashcode
} PartInfo_t;

typedef struct  __attribute__((packed)) {
    AkaiImageHeader_t h;
    uint32_t          deviceCount;
    CompatTable_t     dev[16];
    uint32_t          imgDescLen;
    uint32_t          imgDesc[64];
    PartInfo_t        p;
    uint64_t          partlOffset;        // PARTL tag offset
    uint64_t          xzPartOffset;       // xz Offset from the beginning of the image

} AkaiImage_t;

// Globals
static AkaiImage_t AkaiImg;

// Functions

void compute_sha1(const char *file_path, unsigned char *hashSave) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    unsigned char buffer[8192];
    FILE *file = fopen(file_path, "rb");

    if (!file) {
        perror("Failed to open file");
        return;
    }

    SHA_CTX sha_ctx;
    SHA1_Init(&sha_ctx);

    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) != 0) {
        SHA1_Update(&sha_ctx, buffer, bytes_read);
    }

    SHA1_Final(hash, &sha_ctx);
    fclose(file);

    // Print the SHA-1 hash as a hexadecimal string
    if ( hashSave != NULL ) memcpy(hashSave,hash,SHA_DIGEST_LENGTH) ;

    printf("SHA-1 of %s: ", file_path);
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        printf("%02x ", hash[i]);
    }
    printf("\n");
}


static bool init_decoder(lzma_stream *strm)
{
	lzma_ret ret = lzma_stream_decoder(
			strm, UINT64_MAX, LZMA_CONCATENATED);

	if (ret == LZMA_OK)
		return true;

	const char *msg;
	switch (ret) {
	case LZMA_MEM_ERROR:
		msg = "Memory allocation failed";
		break;

	case LZMA_OPTIONS_ERROR:
		msg = "Unsupported decompressor flags";
		break;

	default:
		msg = "Unknown error, possibly a bug";
		break;
	}

	fprintf(stderr, "Error initializing the decoder: %s (error code %u)\n",
			msg, ret);
	return false;
}


static bool
decompress(lzma_stream *strm, const char *inname, FILE *infile, FILE *outfile)
{
	lzma_action action = LZMA_RUN;

	uint8_t inbuf[BUFSIZ];
	uint8_t outbuf[BUFSIZ];

	strm->next_in = NULL;
	strm->avail_in = 0;
	strm->next_out = outbuf;
	strm->avail_out = sizeof(outbuf);

	while (true) {
		if (strm->avail_in == 0 && !feof(infile)) {
			strm->next_in = inbuf;
			strm->avail_in = fread(inbuf, 1, sizeof(inbuf),
					infile);

			if (ferror(infile)) {
				fprintf(stderr, "%s: Read error: %s\n",
						inname, strerror(errno));
				return false;
			}

			// Once the end of the input file has been reached,
			// we need to tell lzma_code() that no more input
			// will be coming. As said before, this isn't required
			// if the LZMA_CONCATENATED flag isn't used when
			// initializing the decoder.
			if (feof(infile))
				action = LZMA_FINISH;
		}

		lzma_ret ret = lzma_code(strm, action);

		if (strm->avail_out == 0 || ret == LZMA_STREAM_END) {
			size_t write_size = sizeof(outbuf) - strm->avail_out;

			if (fwrite(outbuf, 1, write_size, outfile)
					!= write_size) {
				fprintf(stderr, "Write error: %s\n",
						strerror(errno));
				return false;
			}

			strm->next_out = outbuf;
			strm->avail_out = sizeof(outbuf);
		}

		if (ret != LZMA_OK) {
			if (ret == LZMA_STREAM_END)
				return true;

			const char *msg;
			switch (ret) {
			case LZMA_MEM_ERROR:
				msg = "Memory allocation failed";
				break;

			case LZMA_FORMAT_ERROR:
				// .xz magic bytes weren't found.
				msg = "The input is not in the .xz format";
				break;

			case LZMA_OPTIONS_ERROR:
				msg = "Unsupported compression options";
				break;

			case LZMA_DATA_ERROR:
				msg = "Compressed file is corrupt";
				break;

			case LZMA_BUF_ERROR:
				msg = "Compressed file is truncated or "
						"otherwise corrupt";
				break;

			default:
				// This is most likely LZMA_PROG_ERROR.
				msg = "Unknown error, possibly a bug";
				break;
			}

			fprintf(stderr, "%s: Decoder error: "
					"%s (error code %u)\n",
					inname, msg, ret);
			return false;
		}
	}
}

static bool
init_encoder(lzma_stream *strm)
{
	lzma_ret ret = lzma_easy_encoder(strm, 6, LZMA_CHECK_CRC32);

	if (ret == LZMA_OK)
		return true;

	const char *msg;
	switch (ret) {
	case LZMA_MEM_ERROR:
		msg = "Memory allocation failed";
		break;

	case LZMA_OPTIONS_ERROR:
		msg = "Specified preset is not supported";
		break;

	case LZMA_UNSUPPORTED_CHECK:
		msg = "Specified integrity check is not supported";
		break;

	default:
		msg = "Unknown error, possibly a bug";
		break;
	}

	fprintf(stderr, "Error initializing the encoder: %s (error code %u)\n",
			msg, ret);
	return false;
}

static bool
compress(lzma_stream *strm, FILE *infile, FILE *outfile)
{
	lzma_action action = LZMA_RUN;


    // Compute in file size
    fseek(infile, 0L, SEEK_END);
    size_t inSize =  ftell(infile);
    fseek(infile, 0L, SEEK_SET);


	uint8_t inbuf[BUFSIZ];
	uint8_t outbuf[BUFSIZ];

	strm->next_in = NULL;
	strm->avail_in = 0;
	strm->next_out = outbuf;
	strm->avail_out = sizeof(outbuf);

	while (true) {
		// Fill the input buffer if it is empty.
		if (strm->avail_in == 0 && !feof(infile)) {
			strm->next_in = inbuf;
			strm->avail_in = fread(inbuf, 1, sizeof(inbuf),infile);
			
            if ( ! (ftell(infile) % (1024*1024*10) )) {
                printf(" -> %ld / %ld bytes read     \r", ftell(infile),inSize);
            }

			if (ferror(infile)) {
				fprintf(stderr, "Read error: %s\n",strerror(errno));
				return false;
			}

			if (feof(infile)) {
				action = LZMA_FINISH;
                printf(" -> %lu bytes read (EOF).                          \n",ftell(infile));
            }
		}

		lzma_ret ret = lzma_code(strm, action);

		if (strm->avail_out == 0 || ret == LZMA_STREAM_END) {
			size_t write_size = sizeof(outbuf) - strm->avail_out;

			if (fwrite(outbuf, 1, write_size, outfile)
					!= write_size) {
				fprintf(stderr, "Write error: %s\n",
						strerror(errno));
				return false;
			}

			// Reset next_out and avail_out.
			strm->next_out = outbuf;
			strm->avail_out = sizeof(outbuf);
		}

		if (ret != LZMA_OK) {
			if (ret == LZMA_STREAM_END)
				return true;

			const char *msg;
			switch (ret) {
			case LZMA_MEM_ERROR:
				msg = "Memory allocation failed";
				break;

			case LZMA_DATA_ERROR:
				msg = "File size limits exceeded";
				break;

			default:
				msg = "Unknown error, possibly a bug";
				break;
			}

			fprintf(stderr, "Encoder error: %s (error code %u)\n",
					msg, ret);
			return false;
		}
	}
}

int ExtractRootfsPartition(const char *inImgFilename, const char*outPartFilename ) {

    
    char tempFileName[256];
    strcpy(tempFileName, inImgFilename);
    strcat(tempFileName, ".tmp.xz");
    unlink(tempFileName);

    // Populate Akai image info struct
    if (DisplayAkaiImageInfo(inImgFilename,1)) return 1;

    lzma_stream strm = LZMA_STREAM_INIT;
	
    if (!init_decoder(&strm)) {
        // Decoder initialization failed. There's no point
        // to retry it so we need to exit.
        printf("Error while lzma stream decoder initialization\n");
        return 1;
    }

    compute_sha1(tempFileName,AkaiImg.p.hashCode);

	FILE *inTemp = fopen(tempFileName, "rb");
	if ( !inTemp ) {
        printf("Error opening %s\n", tempFileName);
        return 1;
    }
    
    FILE* outFile = fopen(outPartFilename, "wb");
    if (!outFile) {
        printf("Error while opening out file %s\n", outPartFilename);
        return 1;
    }

    printf("Extracting %s from Akai image file...\n", outPartFilename);
    decompress(&strm, tempFileName, inTemp, outFile); 
    fclose(inTemp);

	// Free the memory allocated for the decoder. This only needs to be
	// done after the last file.
	lzma_end(&strm);

	fclose(outFile);
    unlink(tempFileName);


    printf("Done.\n");

    return 0;
}

int MakeAkaiImage(const char* inImgFilename, const char* inPartFilename, const char* outImgFilename) {

    // Populate Akai image info struct
    if (DisplayAkaiImageInfo(inImgFilename,0)) return 1;

    FILE* outImg = fopen(outImgFilename, "wb");
    if (!outImg) {
        printf("Error while opening out file %s\n", outImgFilename);
        return 1;
    }

	FILE* inImg = fopen(inImgFilename, "rb");
	if ( !inImg ) {
        printf("Error opening %s\n", inImgFilename);
        fclose(outImg);
        unlink(outImgFilename);
        return 1;
    }

	FILE* inPart = fopen(inPartFilename, "rb");
	if ( !inPart ) {
        printf("Error opening %s\n", inPartFilename);
        fclose(outImg);
        fclose(inImg);
        unlink(outImgFilename);
        return 1;
    }

    char tempFileName[256];
    strcpy(tempFileName, inPartFilename);
    strcat(tempFileName, ".tmp.xz");
    unlink(tempFileName);
	FILE* tempFile = fopen(tempFileName, "wb");
	if ( !tempFile ) {
        printf("Error opening %s\n", tempFileName);
        fclose(outImg);
        fclose(inImg);
        fclose(inPart);
        unlink(outImgFilename);
        return 1;
    }
    
    // Encode partition

    printf("Lzma (xz) encoding file %s. Please wait...\n", inPartFilename);
    	
	lzma_stream strm = LZMA_STREAM_INIT;

	// Initialize the encoder. If it succeeds, compress from
	if ( init_encoder(&strm ) ) compress(&strm, inPart, tempFile);  
	lzma_end(&strm);
    fclose(inPart);

    // Reopen xz temp file in read mode

    fclose(tempFile);
	tempFile = fopen(tempFileName, "rb");
	if ( !tempFile ) {
        printf("Error reopening %s\n", tempFileName);
        fclose(outImg);
        fclose(inImg);
        unlink(outImgFilename);
        unlink(tempFileName);
        return 1;
    }

    // Compute xz part size
    fseek(tempFile, 0L, SEEK_END);
    AkaiImg.p.size = ftell(tempFile); 
    fseek(tempFile, 0L, SEEK_SET);

    printf("Size of compressed new rootfs partition is %lu bytes\n", AkaiImg.p.size);

    // Compute sha1
    compute_sha1(tempFileName,AkaiImg.p.hashCode);

    // Write a new Akai Img
    printf("Writing new Akai image %s. Please wait...\n",outImgFilename);

    // Read input img file until PARTL position and write header to the target
    uint8_t bytes[512];
    fread(bytes,AkaiImg.partlOffset , 1, inImg);
    fwrite(bytes,AkaiImg.partlOffset , 1, outImg);

    // Write the new partition info
    fwrite(&(AkaiImg.p), sizeof(AkaiImg.p), 1, outImg);

    // Write the compressed partition file

    size_t s = AkaiImg.p.size;
    while (s) {
        if (s >= sizeof(bytes)) {
            fread(bytes, sizeof(bytes), 1, tempFile);
            fwrite(bytes, sizeof(bytes), 1, outImg);
            s -= sizeof(bytes);
        }
        else {
            fread(bytes, s, 1, tempFile);
            fwrite(bytes, s, 1, outImg);
            break;
        }
    }

    fclose(tempFile);
    unlink(tempFileName);

    // Write EOF (16 bytes) but take 8 bytes alignement into account
    
    uint8_t zeroPad[8];
    uint8_t nbPad = ftell(outImg) % 8;
    memset(zeroPad,0,8);
    
    if ( nbPad) fwrite(zeroPad, 8 - nbPad, 1, outImg);

    fseek(inImg, -16L, SEEK_END);
    fread(bytes, 16, 1, inImg);
    fwrite(bytes, 16, 1, outImg);

    fclose(inImg);
    fclose(outImg);
    
    printf("All operations done. New Akai image %s ready.\n\n",outImgFilename);
    return 0;

}

int DisplayAkaiImageInfo(const char* filename, uint8_t tempFile) {

    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("File %s not found\n", filename);
        fclose(file);
        return 1;
    }

    if (fread(&(AkaiImg.h), sizeof(AkaiImg.h), 1, file) != 1) {
        printf("Error reading image header.\n");
        fclose(file);
        fclose(file);
        return 1;
    }

    // Check if it is the right header
    if (strncmp(AkaiImg.h.header, "AZ01", 4) != 0) {
        printf("Invalid header: AZ01 string not found (%.4s).\n", AkaiImg.h.header);
        fclose(file);
        return 1;
    }

    printf("Image Name            : %s\n", AkaiImg.h.imgName);

    // Compatible devices

    fread(&(AkaiImg.deviceCount), sizeof(AkaiImg.deviceCount), 1, file);
    printf("%d compatible devices  : ", AkaiImg.deviceCount);
    for (int i = 0; i < AkaiImg.deviceCount; i++) {
        fread(&(AkaiImg.dev[i].deviceNameLen), sizeof(AkaiImg.dev[0].deviceNameLen), 1, file);
        fread(&(AkaiImg.dev[i].deviceName)   , sizeof(AkaiImg.dev[0].deviceName)   , 1, file);
        printf("%.*s, ", AkaiImg.dev[i].deviceNameLen, AkaiImg.dev[i].deviceName);
    }
    printf("\n                      : ");

    // USB ids
    fread(&(AkaiImg.deviceCount), sizeof(AkaiImg.deviceCount), 1, file);
    for (int i = 0; i < AkaiImg.deviceCount; i++) {
        fread(&(AkaiImg.dev[i].usbId), sizeof(AkaiImg.dev[0].usbId), 1, file);
        printf("0x%08x, ", AkaiImg.dev[i].usbId);
    }

    printf("\n");

    // Image description
    fread(&(AkaiImg.imgDescLen), sizeof(AkaiImg.imgDescLen), 1, file);      
    fread(&(AkaiImg.imgDesc), AkaiImg.imgDescLen, 1, file);

    // ignore 0 padding and find PARTL tag
    char pad='.';
    while ( pad != 'P') fread(&pad, 1, 1, file);
    fseek(file, -1, SEEK_CUR);

    // Rootfs Partition
    
    printf("Partition Information:\n");
    AkaiImg.partlOffset = ftell(file);

    fread(&(AkaiImg.p), sizeof(AkaiImg.p), 1, file);
    if (strncmp(AkaiImg.p.tag, "PARTL", 5) != 0) {
        printf("'PARTL' rootfs partition tag not found.\n");
        fclose(file);
        return 1;
    }

    printf("  Compressed Size     : %lu bytes\n", AkaiImg.p.size);
    printf("  Partition Name      : %s\n", AkaiImg.p.name);
    printf("  Compression Type    : %.4s\n", AkaiImg.p.compType);
    printf("  Hash Code (%.8s)    : ", AkaiImg.p.hashAlgo);
    
    // Print the hash code as a hexadecimal string
    for (uint32_t i = 0; i < AkaiImg.p.hashLen; i++) {
        printf("%02x ", (uint8_t)AkaiImg.p.hashCode[i]);
    }
    printf("\n");

    AkaiImg.xzPartOffset = ftell(file);
    printf("  XZ Part. offset     : %lu (0x%lx)\n", AkaiImg.xzPartOffset, AkaiImg.xzPartOffset);

    // Extract compressed partiton if needed
    if (tempFile) {
        char tempFileName[128];
        unsigned char bytes[4096];
        strcpy(tempFileName, filename);
        strcat(tempFileName, ".tmp.xz");

        FILE* outTemp = fopen(tempFileName, "wb");
        if (!outTemp) {
            printf("Error opening temp File %s\n", tempFileName);
            fclose(file);
            return 1;
        }

        size_t s = AkaiImg.p.size;
        while (s) {
            if (s >= sizeof(bytes)) {
                fread(bytes, sizeof(bytes), 1, file);
                fwrite(bytes, sizeof(bytes), 1, outTemp);
                s -= sizeof(bytes);
            }
            else {
                fread(bytes, s, 1, file);
                fwrite(bytes, s, 1, outTemp);
                break;
            }
        }

        fclose(outTemp);
    } 
    else fseek(file, AkaiImg.p.size, SEEK_CUR);

    // Take padding into account
   
    // Check EOF tag but take 8 bytes alignment into account
    uint8_t seek =  ftell(file) % 8;
    if ( seek) fseek(file, 8 - seek, SEEK_CUR);

    char eof_marker[3];
    fread(eof_marker, sizeof(char), 3, file);
    if (strncmp(eof_marker, "EOF",3) != 0) {
        printf("'EOF' tag not found. Image file is probably corrupted.\n");
    }

    printf("\n");
    fclose(file);
    return 0;
}


void PrintHelp(void)
{
    printf("Usage : mpcimg2 <action> <FILE>...\n");
    printf("Actions are : \n");
    printf(" -i <Akai image V2 file in>\n");
    printf("    : Display various information about the Akai image\n\n");
    printf(" -x <Akai image V2 file in>\n");
    printf("    : Extraction of the embedded XZ partition file.\n");
    printf("    : Will generate a <your file name>.tmp.xz file in the current directory\n\n");
    printf(" -r <Akai image V2 file in> <rootfs file out>\n");
    printf("    : Uncompress the rootfs partition embedded within the Akai image\n");
    printf("    : The <rootfs file out> extracted is ready to mount on any Linux file system\n\n");
    printf(" -m <Akai image V2 file in> <rootfs file in> <Akai img V2 file out>\n");
    printf("    : Make a new image by providing your own rootfs partition\n");
    printf("    : The modified rootfs parttion size must be exactly the same as the original one\n\n");
    
}

int main(int argc, char* argv[]) {
    
    setbuf(stdout, NULL);

    //printf("---------------------------------------------------------------------\n");
    printf("\nAKAI MPC IMAGE TOOL - V2 - NEW MPC IMG FORMAT (FROM V3.4)\n");
    printf("https://github.com/TheKikGen/MPC-LiveXplore\n");
    printf("(c) The KikGen labs.\n\n");
    //printf("---------------------------------------------------------------------\n");
    
    if (argc < 2) { 
        PrintHelp();
    }

    else 
    if (strncmp(argv[1], "-i",2) == 0) {
        if (argc != 3) {
            PrintHelp();
        }
        else return DisplayAkaiImageInfo(argv[2],0);
    } 

    else 
    if (strncmp(argv[1], "-x",2) == 0) {
        if (argc != 3) {
            PrintHelp();
        }
        else return DisplayAkaiImageInfo(argv[2],1);
    } 
    else 
    
    if (strcmp(argv[1], "-r") == 0) {
        if (argc != 4) {
            PrintHelp();
        } 
        else return ExtractRootfsPartition(argv[2],argv[3]);
    }

    else if (strcmp(argv[1], "-m") == 0) {

        if (argc != 5) {
            PrintHelp();
        }
        else return MakeAkaiImage(argv[2], argv[3], argv[4]);

    }

    return 1;

}
