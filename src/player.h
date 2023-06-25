#include <Arduino.h>
#include "patch_manager.h"

struct sample_player
{
    bool enabled;
    // uint32_t start;
    // uint32_t end;
    int32_t pos;
    float decay;
    float decay_sample;
    bool playing;

    float velocity; // 0.0 -> 1.0
    uint8_t pan; // 0, 9, 18 (L, LR, R) 
    char* filename[64];

    uint32_t numSamples;
    int16_t *sampleStorage;
};

// precompute because easier, L, R
const float pan_lut[2][19] = 
{
    {1.000, 0.996, 0.985, 0.966, 0.940, 0.906, 0.866, 0.819, 0.766, 0.707, 0.643, 0.574, 0.500, 0.423, 0.342, 0.259, 0.174, 0.087, 0.000},
    {0.000, 0.087, 0.174, 0.259, 0.342, 0.423, 0.500, 0.574, 0.643, 0.707, 0.766, 0.819, 0.866, 0.906, 0.940, 0.966, 0.985, 0.996, 1.000}
};

uint32_t totalSampleStorageLen;
uint8_t sampleCount;

struct sample_player samplePlayers[NUM_PLAYERS];

bool playerLoadWav(uint8_t sampleNum, char* filename)
{
    struct sample_player *newPatch = &samplePlayers[sampleNum];
    // newPatch = {}; // reset
    auto freePSRAM =  ESP.getFreePsram();

    if (PatchManager_PrepareSdCard())
    {
        auto dataSize = PatchManager_WaveSize(SD_MMC, filename);
        Serial.print("Datasize: ");
        Serial.println(dataSize);
        if(dataSize > freePSRAM)
        {
            Serial.println("not enough PSRAM memory for sample storage!");
            return false;
        }

        newPatch->numSamples = dataSize / 2;
        newPatch->sampleStorage = (int16_t *)ps_malloc(dataSize);
        if (newPatch->sampleStorage == NULL)
        {
            Serial.printf("Could not allocate psram!\n");
            return false;
        }

        auto readWavSamples = PatchManager_LoadWavefile(SD_MMC, filename, newPatch->sampleStorage, dataSize);
        SD_MMC.end();
        Serial.printf("Read %d samples from %s on SD_MMC\n", readWavSamples, filename);

        if(readWavSamples > 0)
        {
            struct sample_player *newPatch = &samplePlayers[sampleCount];
            sampleCount++;

            newPatch->velocity = 1.0f;
            newPatch->pan = 0.0f;
            memcpy(newPatch->filename, currentFileNameWav, sizeof(currentFileNameWav));
            newPatch->enabled = true;
            newPatch->playing = false;
            newPatch->pos = 0;
            newPatch->decay_sample = 0.0f;
            newPatch->decay = 0.0f;
            newPatch->pan = 9;

            if (newPatch->sampleStorage == NULL)
            {
                Serial.printf("not enough PSRAM memory for sampleStorage!\n");
            }
            Serial.println("Successfully init sample");
        }
        else
        {
            Serial.println("Error reading wav");
        }
    }
    return true;
}

bool playerSetPan(uint8_t sampleNum, uint8_t pan)
{
    if (pan >= 19)
    {
        Serial.println("Invalid pan value");
        return false;
    }
    samplePlayers[sampleNum].pan = pan;
    return true;
}

bool playerSetVol(uint8_t sampleNum, uint8_t vol)
{
    if (vol >= 16)
    {
        Serial.println("Invalid vol value");
        return false;
    }
    samplePlayers[sampleNum].velocity = (float)vol / 16;
    return true;
}

bool playerSampleOn(uint8_t sampleNum)
{
    struct sample_player *player = &samplePlayers[sampleNum];
    if(player->enabled)
    {
        if(player->playing)
        {
            player->decay_sample = ((float)player->sampleStorage[player->pos]) / ((float)0x8000) * player->velocity;
            player->pos = 0;
            return true;
        }
        player->playing = true;
        return true;
    }
    return false;
}

void playerProcess(float *signal_l, float *signal_r, const int buffLen)
{
    for (int i = 0; i < sampleCount; i++)
    {
        struct sample_player *player = &samplePlayers[i];

        for (int n = 0; n < buffLen; n++)
        {
            float sample_f = 0;
            if (player->decay_sample != 0.0)
            {
                if (player->decay_sample > AUDIBLE_LIMIT)
                {
                    player->decay_sample *= 0.99;
                    sample_f += player->decay_sample;
                }
                else
                {
                    player->decay_sample = 0;
                }
            }
            if (player->playing)
            {
                sample_f += ((float)player->sampleStorage[player->pos]) / ((float)0x8000);
                sample_f *= player->velocity;
                player->pos += 1;
                if (player->pos >= player->numSamples)
                {
                    player->playing = false;
                    player->decay_sample = sample_f;
                    player->pos = 0;
                }
                // panning
                signal_l[n] += sample_f * pan_lut[0][player->pan];
                signal_r[n] += sample_f * pan_lut[1][player->pan];
            }
        }
    }
}

void playerInit()
{
    psramInit();
    Serial.printf("Total PSRAM: %d\n", ESP.getPsramSize());
    Serial.printf("Free PSRAM: %d\n", ESP.getFreePsram());

    totalSampleStorageLen = ESP.getFreePsram() / sizeof(int16_t);
}