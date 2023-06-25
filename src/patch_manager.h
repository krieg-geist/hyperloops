/*
 * this file contains the implementation of the "patch" manager
 * It allows loading and saving the samples including parameters
 *
 * Saved data will appear as pairs: .wav and .bin
 * The parameters are in the .bin file
 *
 * Hot swap of sd card is supported. SD is busy only during read/write operation
 *
 * Support of loading wav files without parameters
 * - in case no parameters are existing default values are used
 *
 * only signed 16 bit wav files are supported at the moment
 * sampleRate is ignored -> you can pitch it afterwards
 *​
 * Author: Marcel Licence
 */
#pragma once

#include <Arduino.h>
#include <FS.h>
#include <LITTLEFS.h>
#include <SD_MMC.h>

enum patchDst
{
    patch_dest_littlefs,
    patch_dest_sd_mmc,
};

struct patchParamV0_s
{
    float pitch;
    float loop_start;
    float loop_end;
};

struct patchParamV1_s
{
    float attack;
    float decay;
    float sustain;
    float release;
};

struct patchParam_s
{
    union
    {
        struct
        {
            uint32_t version;

            struct patchParamV0_s patchParamV0;
            struct patchParamV1_s patchParamV1;

        };
        uint8_t buffer[512]; /*! raw data */
    };
    char filename[64];
};

/*
 * union is very handy for easy conversion of bytes to the wav header information
 */
union wavHeader
{
    struct
    {
        char riff[4]; /*!< 'RIFF' */
        uint32_t fileSize; /*!< bytes to write containing all data (header + data) */
        char waveType[4]; /*!< 'WAVE' */

        char format[4]; /*!< 'fmt ' */
        uint32_t lengthOfData; /*!< length of the fmt header (16 bytes) */
        uint16_t format_tag; /*!< 0x0001: PCM */
        uint16_t numberOfChannels; /*!< 'WAVE' */
        uint32_t sampleRate;
        uint32_t byteRate;

        uint16_t bytesPerSample;
        uint16_t bitsPerSample;
        char dataStr[4];
        uint32_t dataSize;
    };
    uint8_t wavHdr[44];
};


uint32_t patch_selectedFileIndex = 0;
char currentFileNameWav[64] = "/samples/testSample.wav\0";
char currentFileNameBin[64] = "/samples/testSample.bin\0";


enum patchDst patchManagerDest = patch_dest_littlefs;

/*
 * last written files
 */
char wavNewFileName[64];
char parNewFileName[64];


void PatchManager_Init(void)
{
    /* nothing to do */
}

int PatchManager_GetFileList(fs::FS &fs, const char *dirname, void(*fileInd)(char *filename, int offset), int offset)
{
#ifdef PATCHMANAGER_DEBUG
    Serial.printf("Listing directory: %s\n", dirname);
#endif
    File root = fs.open(dirname);
    if (!root)
    {
        Serial.println("Failed to open directory");
        return 0;
    }
    if (!root.isDirectory())
    {
        Serial.println("Not a directory");
        return 0;
    }

    File file = root.openNextFile();


    int foundFiles = 0;

    while (file)
    {
        if (file.isDirectory())
        {
#if 0
            Serial.print("  DIR : ");
            Serial.println(file.name());
#endif
        }
        else
        {
#if 0
            Serial.printf("%03d - FILE: ", patch_selectedFileIndex);
            Serial.print(file.name());
            Serial.print("  SIZE: ");

            Serial.println(file.size());
#endif
            strcpy(currentFileNameWav, file.name());
            strcpy(currentFileNameBin, file.name());
            strcpy(&currentFileNameBin[strlen(currentFileNameBin) - 3], "bin");

            if (strcmp(".wav", &file.name()[strlen(file.name()) - 4]) == 0)
            {
#if 0
                Serial.printf("ignore %s, %s\n", ".wav", &file.name()[strlen(file.name()) - 4]);
#endif


                if (offset > 0)
                {
                    offset--;
                }
                else
                {
                    fileInd(currentFileNameWav, foundFiles++);
                }
            }
        }
        file = root.openNextFile();
    }
    return foundFiles;
}


bool PatchManager_PrepareSdCard(void)
{

    if (!SD_MMC.begin("/sdcard", true)) /* makes less noise on recording! */
    {
        Serial.println("Card Mount Failed");
        delay(1000);
        return false;
    }

    uint8_t cardType = SD_MMC.cardType();

    if (cardType == CARD_NONE)
    {
        Serial.println("No SD card attached");

        delay(1000);
        return false;
    }

    if (cardType == CARD_MMC)
    {
        Serial.println("Card Access: MMC");
    }
    else if (cardType == CARD_SD)
    {
        Serial.println("Card Access: SDSC");
    }
    else if (cardType == CARD_SDHC)
    {
        Serial.println("Card Access: SDHC");
    }
    else
    {
        Serial.println("Card Access: UNKNOWN");
    }
    Serial.println("SD card prep OK");
    return true;
}

bool PatchManager_PrepareLittleFs(void)
{
    if (!LITTLEFS.begin())
    {
        Serial.println("LITTLEFS Mount Failed");
        return false;
    }

    return true;
}

int PatchManager_GetFileListExt(void(*fileInd)(char *filename, int offset), int offset)
{
    if (patchManagerDest == patch_dest_sd_mmc)
    {
        if (PatchManager_PrepareSdCard())
        {
            return PatchManager_GetFileList(SD_MMC, "/samples", fileInd, offset);
        }
    }
    else
    {
        if (PatchManager_PrepareLittleFs())
        {
            return PatchManager_GetFileList(LITTLEFS, "/samples", fileInd, offset);
        }
    }
    return 0;
}

void PatchManager_FilenameFromIdx(fs::FS &fs, const char *dirname, uint8_t index)
{
#ifdef PATCHMANAGER_DEBUG
    Serial.printf("Listing directory: %s\n", dirname);
#endif
    File root = fs.open(dirname);
    if (!root)
    {
        Serial.println("Failed to open directory");
        return;
    }
    if (!root.isDirectory())
    {
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    patch_selectedFileIndex = 0;
    while (file)
    {
        if (file.isDirectory())
        {
#if 0
            Serial.print("  DIR : ");
            Serial.println(file.name());
#endif
        }
        else
        {
#if 0
            Serial.printf("%03d - FILE: ", patch_selectedFileIndex);
            Serial.print(file.name());
            Serial.print("  SIZE: ");

            Serial.println(file.size());
#endif
            strcpy(currentFileNameWav, file.name());
            strcpy(currentFileNameBin, file.name());
            strcpy(&currentFileNameBin[strlen(currentFileNameBin) - 3], "bin");

            if (strcmp(".wav", &file.name()[strlen(file.name()) - 4]) == 0)
            {
#if 0
                Serial.printf("ignore %s, %s\n", ".wav", &file.name()[strlen(file.name()) - 4]);
#endif
                if (index > 0)
                {
                    index--;
                }
                else
                {
                    return;
                }
                patch_selectedFileIndex++;
            }


        }
        file = root.openNextFile();
    }
    patch_selectedFileIndex--;
}

char lastSelectedFile[128] = "";

void PatchManager_UpdateFilename(void)
{
    if (patchManagerDest == patch_dest_sd_mmc)
    {
        if (PatchManager_PrepareSdCard())
        {
            PatchManager_FilenameFromIdx(SD_MMC, "/samples", patch_selectedFileIndex);
            SD_MMC.end();
#ifdef PATCHMANAGER_DEBUG
            Serial.printf("Active file: %03d - %s\n", patch_selectedFileIndex, currentFileNameWav);
            Serial.printf("Active file: %03d - %s\n", patch_selectedFileIndex, currentFileNameBin);
#endif
            sprintf(lastSelectedFile, "SD_MMC: %s", currentFileNameWav);
            Serial.println(lastSelectedFile);
            sprintf(lastSelectedFile, "%s", currentFileNameWav);
        }
    }
    else
    {
        if (PatchManager_PrepareLittleFs())
        {
            PatchManager_FilenameFromIdx(LITTLEFS, "/samples", patch_selectedFileIndex);
            LITTLEFS.end();
#ifdef PATCHMANAGER_DEBUG
            Serial.printf("Active file: %03d - %s\n", patch_selectedFileIndex, currentFileNameWav);
            Serial.printf("Active file: %03d - %s\n", patch_selectedFileIndex, currentFileNameBin);
#endif
            sprintf(lastSelectedFile, "LITTLEFS: %s", currentFileNameWav);
            Serial.println(lastSelectedFile);
            sprintf(lastSelectedFile, "%s", currentFileNameWav);
        }
    }
}

void PatchManager_FileIdxInc(uint8_t unused, float value)
{
    if (value > 0)
    {
        patch_selectedFileIndex++;
        PatchManager_UpdateFilename();
    }
}

void PatchManager_FileIdxDec(uint8_t unused, float value)
{
    if (value > 0)
    {
        if (patch_selectedFileIndex > 0)
        {
            patch_selectedFileIndex--;
        }
        PatchManager_UpdateFilename();
    }
}

void PatchManager_SaveWavefile(fs::FS &fs, char *filename, int16_t *buffer, uint32_t bufferSize)
{
    File f = fs.open(filename, FILE_WRITE);
    if (!f)
    {
        Serial.println("Could not create new file\n");
        return;
    }

    uint32_t dataSizeOfbuffer = sizeof(int16_t) * bufferSize;
    union wavHeader wavHeader;

    memcpy(wavHeader.riff, "RIFF", 4);
    wavHeader.fileSize = 44 + dataSizeOfbuffer;
    memcpy(wavHeader.waveType, "WAVE", 4);
    memcpy(wavHeader.format, "fmt ", 4);
    wavHeader.lengthOfData = 16; /* length of the fmt header */
    wavHeader.format_tag = 0x0001; /* 0x0001: PCM */
    wavHeader.numberOfChannels = 1;
    wavHeader.sampleRate = 44100;
    wavHeader.byteRate = 44100 * 2;
    wavHeader.bytesPerSample = 2;
    wavHeader.bitsPerSample = 16;

    memcpy(wavHeader.dataStr, "data", 4);
    wavHeader.dataSize = dataSizeOfbuffer;

    f.seek(0, SeekSet);
    f.write(wavHeader.wavHdr, 44);

    /* avoid watchdog */
    delay(1);

    f.write((uint8_t *)buffer, dataSizeOfbuffer);
    f.close();

    /* avoid watchdog */
    delay(1);
}

uint32_t PatchManager_WaveSize(fs::FS &fs, char *filename)
{
    File f = fs.open(filename, FILE_READ);
    if (!f)
    {
        Serial.println("Could not read file\n");
        return 0;
    }

    union wavHeader wavHeader;

    memset(wavHeader.wavHdr, 0, sizeof(wavHeader));

    f.seek(0, SeekSet);
    f.read(wavHeader.wavHdr, 44);

    /* avoid watchdog */
    delay(1);
    return wavHeader.dataSize / wavHeader.numberOfChannels;
}


uint32_t PatchManager_LoadWavefile(fs::FS &fs, char *filename, int16_t *buffer, uint32_t bufferSize)
{
    File f = fs.open(filename, FILE_READ);
    if (!f)
    {
        Serial.println("Could not read file\n");
        return 0;
    }

    union wavHeader wavHeader;

    memset(wavHeader.wavHdr, 0, sizeof(wavHeader));

    f.seek(0, SeekSet);
    f.read(wavHeader.wavHdr, 44);

    /* avoid watchdog */
    delay(1);

    uint32_t bufferIn = 0;

    if (wavHeader.format_tag != 1)
        return 0;
    if (wavHeader.bitsPerSample != 16)
        return 0;
    if (wavHeader.sampleRate != 44100)
        return 0;

    if (wavHeader.numberOfChannels == 1)
    {
        f.read((uint8_t *)buffer, wavHeader.dataSize);
        bufferIn += wavHeader.dataSize;
        Serial.println("Mono");
    }
    else
    {
        uint32_t dataLeft = wavHeader.dataSize;
        uint32_t dataRead;

        while (true)
        {
            uint8_t tempBuffer[512];
            if (dataLeft > 512)
            {
                dataRead = 512;
            }
            else
            {
                dataRead = dataLeft;
            }

            f.read(tempBuffer, dataRead);
            dataLeft -= dataRead;

            for (int i = 0; i < dataRead; i++)
            {
                /*
                 * reading only the left channel
                 * right channel will be ignored and data just skipped
                 */
                ((uint8_t *)buffer)[bufferIn] = tempBuffer[i];
                ((uint8_t *)buffer)[bufferIn+1] = tempBuffer[i+1];
                bufferIn += 2;
                i += 4;
            }

            if (dataLeft == 0)
            {
                break;
            }
        }
        Serial.println("Stereo");
    }
    f.close();

    /* avoid watchdog */
    delay(1);

    return bufferIn / sizeof(int16_t);
}

void PatchManager_CreateDir(fs::FS &fs, const char *path)
{
    Serial.printf("Creating Dir: %s\n", path);
    if (fs.mkdir(path))
    {
        Serial.println("Dir created");
    }
    else
    {
        Serial.println("mkdir failed");
    }
}

void PatchManager_SavePatchParam(fs::FS &fs, char *filename, struct patchParam_s *patchParam)
{
    File f = fs.open(filename, FILE_WRITE);
    if (!f)
    {
        Serial.println("Could not create new file\n");
        return;
    }

    f.write((uint8_t *)patchParam, sizeof(*patchParam));

    f.close();
}

void PatchManager_LoadPatchParam(fs::FS &fs, char *filename, struct patchParam_s *patchParam)
{
    File f = fs.open(filename, FILE_READ);
    if (!f)
    {
        Serial.printf("No patch parameter\n");
        patchParam->version = 0;
        patchParam->patchParamV0.pitch = 1;
        patchParam->patchParamV0.loop_start = 0;
        patchParam->patchParamV0.loop_end = 0xFFFFFFFF;
        return;
    }

    f.read((uint8_t *)patchParam, sizeof(*patchParam));

#ifdef PATCHMANAGER_DEBUG
    Serial.printf("patchParam:\n");
    Serial.printf("    version: %d\n", patchParam->version);

    Serial.printf("        pitch: %0.06f\n", patchParam->patchParamV0.pitch);
    Serial.printf("        loop_start: %0.06f\n", patchParam->patchParamV0.loop_start);
    Serial.printf("        loop_end: %0.06f\n", patchParam->patchParamV0.loop_end);

    if (patchParam->version >= 1)
    {
        Serial.printf("        attack: %0.06f\n", patchParam->patchParamV1.attack);
        Serial.printf("        decay: %0.06f\n", patchParam->patchParamV1.decay);
        Serial.printf("        sustain: %0.06f\n", patchParam->patchParamV1.sustain);
        Serial.printf("        release: %0.06f\n", patchParam->patchParamV1.release);
    }
#endif

    f.close();
}

void PatchManager_SetDestination(uint8_t destination, float value)
{
    if (value > 0)
    {
        switch (destination)
        {
        case 0:
            patchManagerDest = patch_dest_littlefs;
            Serial.println("Patch storage: little fs");
            break;
        case 1:
            patchManagerDest = patch_dest_sd_mmc;
            Serial.println("Patch storage: sd mmc");
            break;
        }
        PatchManager_UpdateFilename();
    }
}

void PatchManager_CreateNewFileNames(fs::FS &fs)
{

    int i = 0;

    while (true)
    {
        sprintf(wavNewFileName, "/samples/newSample%03d.wav", i);
        sprintf(parNewFileName, "/samples/newSample%03d.bin", i);

        if (fs.exists(wavNewFileName))
        {
            i++;
            continue;
        }

        if (fs.exists(parNewFileName))
        {
            i++;
            continue;
        }

        break;
    }
}

void PatchManager_SaveNewPatch(struct patchParam_s *patchParam, int16_t *buffer, int bufferSize)
{
    if (patchManagerDest == patch_dest_sd_mmc)
    {
        if (PatchManager_PrepareSdCard())
        {
            PatchManager_CreateDir(SD_MMC, "/samples");
            PatchManager_CreateNewFileNames(SD_MMC);
            PatchManager_SavePatchParam(SD_MMC, parNewFileName, patchParam);
            PatchManager_SaveWavefile(SD_MMC, wavNewFileName, buffer, bufferSize);
            SD_MMC.end();
            Serial.printf("Written %d to %s on SD_MMC\n", bufferSize, wavNewFileName);
        }
    }
    else
    {
        if (PatchManager_PrepareLittleFs())
        {
            PatchManager_CreateDir(LITTLEFS, "/samples");
            PatchManager_CreateNewFileNames(LITTLEFS);
            PatchManager_SavePatchParam(LITTLEFS, parNewFileName, patchParam);
            PatchManager_SaveWavefile(LITTLEFS, wavNewFileName, buffer, bufferSize);
            LITTLEFS.end();
            Serial.printf("Written %d to %s on LITTLEFS\n", bufferSize, wavNewFileName);
        }
    }
}

void PatchManager_SetFilename(const char *filename)
{
    strcpy(currentFileNameWav, filename);
    strcpy(currentFileNameBin, filename);
    strcpy(&currentFileNameBin[strlen(currentFileNameBin) - 3], "bin");
}

uint32_t PatchManager_LoadPatch(struct patchParam_s *patchParam, int16_t *buffer, int bufferSize)
{
    memset(patchParam, 0, sizeof(*patchParam));

    uint32_t readBufferBytes = 0 ;

    if (patchManagerDest == patch_dest_sd_mmc)
    {
        if (PatchManager_PrepareSdCard())
        {
            PatchManager_LoadPatchParam(SD_MMC, currentFileNameBin, patchParam);

            readBufferBytes = PatchManager_LoadWavefile(SD_MMC, currentFileNameWav, buffer, bufferSize);
            SD_MMC.end();
            Serial.printf("Read %d from %s on SD_MMC\n", readBufferBytes, currentFileNameWav);
        }
    }
    else
    {
        if (PatchManager_PrepareLittleFs())
        {
            PatchManager_LoadPatchParam(LITTLEFS, currentFileNameBin, patchParam);

            readBufferBytes = PatchManager_LoadWavefile(LITTLEFS, currentFileNameWav, buffer, bufferSize);
            LITTLEFS.end();
            Serial.printf("Read %d from %s on LITTLEFS\n", readBufferBytes, currentFileNameWav);
        }
    }

    memcpy(patchParam->filename, currentFileNameBin, sizeof(patchParam->filename));

    return readBufferBytes;
}