import wave
import numpy as np
import glob, os
import sys
import soundfile as sf

def save_wav_channel(fn, wav, channel):
    '''
    Take Wave_read object as an input and save one of its
    channels into a separate .wav file.
    '''
    # Read data
    nch   = wav.getnchannels()
    depth = wav.getsampwidth()
    wav.setpos(0)
    sdata = wav.readframes(wav.getnframes())

    # Extract channel data (24-bit data not supported)
    typ = { 1: np.uint8, 2: np.uint16, 4: np.uint32 }.get(depth)
    if not typ:
        raise ValueError("sample width {} not supported".format(depth))
    if channel >= nch:
        raise ValueError("cannot extract channel {} out of {}".format(channel+1, nch))
    print ("Extracting channel {} out of {} channels, {}-bit depth".format(channel+1, nch, depth*8))
    data = np.fromstring(sdata, dtype=typ)
    ch_data = data[channel::nch]

    # Save channel to a separate file
    outwav = wave.open(fn, 'w')
    outwav.setnchannels(1)
    outwav.setframerate(44100)
    outwav.setsampwidth(2)
    outwav.writeframes(ch_data.tostring())
    outwav.close()

os.chdir(sys.argv[1])

wavfiles = []
for file in glob.glob("*.wav"):
    wavfiles.append(file)

wavnum = 0
if not os.path.exists('output'):
    os.makedirs('output')

for file in wavfiles:
    #wav = wave.open(file)
    #save_wav_channel('output/{}'.format(wavnum), wav, 0)
    data, samplerate = sf.read(file)
    sf.write('output/' + str(wavnum) + '.wav', data, samplerate=44100, subtype='PCM_16')
    wavnum+= 1
