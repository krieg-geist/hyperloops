#include <Arduino.h>

uint32_t ReverbMemoryAddress(uint32_t address)
{
  // Ensures address does not leave the reverb work area.
  static constexpr uint32_t MASK = (RAM_SIZE - 1) / 2;
  uint32_t offset = s_reverb_current_address + (address & MASK);
  offset += s_reverb_base_address & ((int32_t)(offset << 13) >> 31);

  // We address RAM in bytes. TODO: Change this to words.
  return (offset & MASK) * 2u;
}

int16_t ReverbRead(uint32_t address, int32_t offset)
{
  // TODO: This should check interrupts.
  const uint32_t real_address = ReverbMemoryAddress((address << 2) + offset);

  int16_t data;
  memcpy(&data, &s_ram[real_address], sizeof(data));
  return data;
}

void ReverbWrite(uint32_t address, int16_t data)
{
  // TODO: This should check interrupts.
  const uint32_t real_address = ReverbMemoryAddress(address << 2);
  memcpy(&s_ram[real_address], &data, sizeof(data));
}

// Zeroes optimized out; middle removed too(it's 16384)
static constexpr array<int16_t, 20> s_reverb_resample_coefficients = {
  -1, 2, -10, 35, -103, 266, -616, 1332, -2960, 10246, 10246, -2960, 1332, -616, 266, -103, 35, -10, 2, -1,
};
static int16_t s_last_reverb_input[2];
static int32_t s_last_reverb_output[2];

inline static int32_t Reverb4422(const int16_t* src)
{
  int32_t out = 0; // 32-bits is adequate(it won't overflow)
  for (uint32_t i = 0; i < 20; i++)
    out += s_reverb_resample_coefficients[i] * src[i * 2];

  // Middle non-zero
  out += 0x4000 * src[19];
  out >>= 15;
  return clamp<int32_t>(out, -32768, 32767);
}

template<bool phase>
inline static int32_t Reverb2244(const int16_t* src)
{
  int32_t out; // 32-bits is adequate(it won't overflow)
  if (phase)
  {
    // Middle non-zero
    out = src[9];
  }
  else
  {
    out = 0;
    for (uint32_t i = 0; i < 20; i++)
      out += s_reverb_resample_coefficients[i] * src[i];

    out >>= 14;
    out = clamp<int32_t>(out, -32768, 32767);
  }

  return out;
}

inline static int16_t ReverbSat(int32_t val)
{
  return static_cast<int16_t>(clamp<int32_t>(val, -0x8000, 0x7FFF));
}

inline static int16_t ReverbNeg(int16_t samp)
{
  if (samp == -32768)
    return 0x7FFF;

  return -samp;
}

inline static int32_t IIASM(const int16_t IIR_ALPHA, const int16_t insamp)
{
  if (IIR_ALPHA == -32768)
  {
    if (insamp == -32768)
      return 0;
    else
      return insamp * -65536;
  }
  else
    return insamp * (32768 - IIR_ALPHA);
}

void ProcessReverb(int16_t left_in, int16_t right_in, int32_t* left_out, int32_t* right_out)
{
  s_last_reverb_input[0] = left_in;
  s_last_reverb_input[1] = right_in;
  s_reverb_downsample_buffer[0][s_reverb_resample_buffer_position | 0x00] = left_in;
  s_reverb_downsample_buffer[0][s_reverb_resample_buffer_position | 0x40] = left_in;
  s_reverb_downsample_buffer[1][s_reverb_resample_buffer_position | 0x00] = right_in;
  s_reverb_downsample_buffer[1][s_reverb_resample_buffer_position | 0x40] = right_in;

  int32_t out[2];
  if (s_reverb_resample_buffer_position & 1u)
  {
    array<int32_t, 2> downsampled;
    for (unsigned lr = 0; lr < 2; lr++)
      downsampled[lr] = Reverb4422(&s_reverb_downsample_buffer[lr][(s_reverb_resample_buffer_position - 38) & 0x3F]);

    for (unsigned lr = 0; lr < 2; lr++)
    {
      if (s_SPUCNT.reverb_master_enable)
      {
        const int16_t IIR_INPUT_A =
          ReverbSat((((ReverbRead(s_reverb_registers.IIR_SRC_A[lr ^ 0]) * s_reverb_registers.IIR_COEF) >> 14) +
                     ((downsampled[lr] * s_reverb_registers.IN_COEF[lr]) >> 14)) >>
                    1);
        const int16_t IIR_INPUT_B =
          ReverbSat((((ReverbRead(s_reverb_registers.IIR_SRC_B[lr ^ 1]) * s_reverb_registers.IIR_COEF) >> 14) +
                     ((downsampled[lr] * s_reverb_registers.IN_COEF[lr]) >> 14)) >>
                    1);
        const int16_t IIR_A =
          ReverbSat((((IIR_INPUT_A * s_reverb_registers.IIR_ALPHA) >> 14) +
                     (IIASM(s_reverb_registers.IIR_ALPHA, ReverbRead(s_reverb_registers.IIR_DEST_A[lr], -1)) >> 14)) >>
                    1);
        const int16_t IIR_B =
          ReverbSat((((IIR_INPUT_B * s_reverb_registers.IIR_ALPHA) >> 14) +
                     (IIASM(s_reverb_registers.IIR_ALPHA, ReverbRead(s_reverb_registers.IIR_DEST_B[lr], -1)) >> 14)) >>
                    1);

        ReverbWrite(s_reverb_registers.IIR_DEST_A[lr], IIR_A);
        ReverbWrite(s_reverb_registers.IIR_DEST_B[lr], IIR_B);
      }

      const int32_t ACC = ((ReverbRead(s_reverb_registers.ACC_SRC_A[lr]) * s_reverb_registers.ACC_COEF_A) >> 14) +
                      ((ReverbRead(s_reverb_registers.ACC_SRC_B[lr]) * s_reverb_registers.ACC_COEF_B) >> 14) +
                      ((ReverbRead(s_reverb_registers.ACC_SRC_C[lr]) * s_reverb_registers.ACC_COEF_C) >> 14) +
                      ((ReverbRead(s_reverb_registers.ACC_SRC_D[lr]) * s_reverb_registers.ACC_COEF_D) >> 14);

      const int16_t FB_A = ReverbRead(s_reverb_registers.MIX_DEST_A[lr] - s_reverb_registers.FB_SRC_A);
      const int16_t FB_B = ReverbRead(s_reverb_registers.MIX_DEST_B[lr] - s_reverb_registers.FB_SRC_B);
      const int16_t MDA = ReverbSat((ACC + ((FB_A * ReverbNeg(s_reverb_registers.FB_ALPHA)) >> 14)) >> 1);
      const int16_t MDB = ReverbSat(
        FB_A +
        ((((MDA * s_reverb_registers.FB_ALPHA) >> 14) + ((FB_B * ReverbNeg(s_reverb_registers.FB_X)) >> 14)) >> 1));
      const int16_t IVB = ReverbSat(FB_B + ((MDB * s_reverb_registers.FB_X) >> 15));

      if (s_SPUCNT.reverb_master_enable)
      {
        ReverbWrite(s_reverb_registers.MIX_DEST_A[lr], MDA);
        ReverbWrite(s_reverb_registers.MIX_DEST_B[lr], MDB);
      }

      s_reverb_upsample_buffer[lr][(s_reverb_resample_buffer_position >> 1) | 0x20] =
        s_reverb_upsample_buffer[lr][s_reverb_resample_buffer_position >> 1] = IVB;
    }

    s_reverb_current_address = (s_reverb_current_address + 1) & 0x3FFFFu;
    if (s_reverb_current_address == 0)
      s_reverb_current_address = s_reverb_base_address;

    for (unsigned lr = 0; lr < 2; lr++)
      out[lr] =
        Reverb2244<false>(&s_reverb_upsample_buffer[lr][((s_reverb_resample_buffer_position >> 1) - 19) & 0x1F]);
  }
  else
  {
    for (unsigned lr = 0; lr < 2; lr++)
      out[lr] = Reverb2244<true>(&s_reverb_upsample_buffer[lr][((s_reverb_resample_buffer_position >> 1) - 19) & 0x1F]);
  }

  s_reverb_resample_buffer_position = (s_reverb_resample_buffer_position + 1) & 0x3F;

  s_last_reverb_output[0] = *left_out = ApplyVolume(out[0], s_reverb_registers.vLOUT);
  s_last_reverb_output[1] = *right_out = ApplyVolume(out[1], s_reverb_registers.vROUT);
}

void Reverb_Process(float *signal_l, float *signal_r, int buffLen)
{
    float inSample[buffLen];
    for (int n = 0; n < buffLen; n++)
    {

    }
}
