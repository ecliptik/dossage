// Offline host-native renderer for Passage's music.
//
// Drives the SAME synthesis code the DOS build ships (musicPlayer.cpp,
// Timbre.cpp, Envelope.cpp, unmodified) through one full loop of the
// song's note grid, capturing the mixed PCM output to a WAV file instead
// of a live audio device. This is a capture of the existing sound, not a
// reimplementation -- see PLAN.md / the audio-rewrite plan for why.
//
// Must be run with its working directory set to
// vendor/passage/gameSource/ (same relative-path convention
// readTGA("music", ...) already assumes).

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <vector>

// musicPlayer.cpp's globals/functions we need. Not declared in
// musicPlayer.h (which only exposes the game-facing API), so declared
// here to match musicPlayer.cpp exactly.
extern int sampleRate;
extern double entireGridDuraton;
extern double musicLoudness;
void loadMusicImage( const char *inTGAFileName );
// The original per-sample synthesis (musicPlayer.cpp), kept under this
// name specifically so this offline tool can still drive it after
// musicPlayer.cpp's live `audioCallback` was replaced with pre-rendered
// playback.
void synthesizeAudioCallback( void *inUserData, SDL_AudioStream *inStream,
                    int inAdditionalAmount, int inTotalAmount );


static void writeWavHeader( FILE *inFile, Uint32 inDataBytes,
                            int inSampleRate, int inChannels,
                            int inBitsPerSample ) {
    Uint32 byteRate = inSampleRate * inChannels * ( inBitsPerSample / 8 );
    Uint16 blockAlign = (Uint16)( inChannels * ( inBitsPerSample / 8 ) );
    Uint32 riffSize = 36 + inDataBytes;

    fwrite( "RIFF", 1, 4, inFile );
    fwrite( &riffSize, 4, 1, inFile );
    fwrite( "WAVE", 1, 4, inFile );

    fwrite( "fmt ", 1, 4, inFile );
    Uint32 fmtSize = 16;
    fwrite( &fmtSize, 4, 1, inFile );
    Uint16 audioFormat = 1; // PCM
    fwrite( &audioFormat, 2, 1, inFile );
    Uint16 numChannels = (Uint16)inChannels;
    fwrite( &numChannels, 2, 1, inFile );
    Uint32 sr = (Uint32)inSampleRate;
    fwrite( &sr, 4, 1, inFile );
    fwrite( &byteRate, 4, 1, inFile );
    fwrite( &blockAlign, 2, 1, inFile );
    Uint16 bps = (Uint16)inBitsPerSample;
    fwrite( &bps, 2, 1, inFile );

    fwrite( "data", 1, 4, inFile );
    fwrite( &inDataBytes, 4, 1, inFile );
    }



int main( int inNumArgs, char **inArgs ) {
    const char *outPath = "SONG.WAV";
    if( inNumArgs > 1 ) {
        outPath = inArgs[1];
        }

    // Fixed seed so re-running this tool produces a byte-identical WAV
    // (the only non-deterministic part of the synth is the noise
    // timbre's rand() calls).
    srand( 1 );

    // loadMusicImage sets entireGridDuraton and sampleRate is fixed at
    // file scope (22050) -- both pure computation, no device I/O.
    loadMusicImage( "music.tga" );

    musicLoudness = 1.0;

    const Sint64 totalSamples =
        (Sint64)( entireGridDuraton * sampleRate + 0.5 );
    const Sint64 totalBytes = totalSamples * 4; // S16 stereo

    printf( "Rendering %.2f seconds (%lld samples, %lld bytes) at "
            "%d Hz stereo S16...\n",
            entireGridDuraton, (long long)totalSamples,
            (long long)totalBytes, sampleRate );

    SDL_AudioSpec spec;
    spec.freq = sampleRate;
    spec.format = SDL_AUDIO_S16;
    spec.channels = 2;

    // A pure logical stream (no device attached) -- audioCallback only
    // ever calls SDL_PutAudioStreamData on it, which we drain via
    // SDL_GetAudioStreamData right after each call so memory doesn't
    // grow unbounded across a 136-second render.
    SDL_AudioStream *stream = SDL_CreateAudioStream( &spec, &spec );
    if( stream == NULL ) {
        fprintf( stderr, "SDL_CreateAudioStream failed: %s\n",
                 SDL_GetError() );
        return 1;
        }

    std::vector<Uint8> output;
    output.reserve( (size_t)totalBytes );

    const int chunkBytes = 4096; // bytes requested per callback call
    Sint64 producedBytes = 0;
    Uint8 drainBuf[ chunkBytes * 4 ];

    while( producedBytes < totalBytes ) {
        int want = chunkBytes;
        if( producedBytes + want > totalBytes ) {
            want = (int)( totalBytes - producedBytes );
            // audioCallback rounds up to a whole number of frames
            // internally; ask for at least one full frame.
            if( want < 4 ) {
                want = 4;
                }
            }

        synthesizeAudioCallback( NULL, stream, want, want );

        int avail = SDL_GetAudioStreamAvailable( stream );
        while( avail > 0 ) {
            int toRead = avail;
            if( toRead > (int)sizeof( drainBuf ) ) {
                toRead = (int)sizeof( drainBuf );
                }
            int got = SDL_GetAudioStreamData( stream, drainBuf, toRead );
            if( got <= 0 ) {
                break;
                }
            output.insert( output.end(), drainBuf, drainBuf + got );
            producedBytes += got;
            avail = SDL_GetAudioStreamAvailable( stream );
            }
        }

    // Trim to exactly totalBytes (the last callback iteration may have
    // produced a little more than requested, since audioCallback rounds
    // up to whole frames).
    if( (Sint64)output.size() > totalBytes ) {
        output.resize( (size_t)totalBytes );
        }

    SDL_DestroyAudioStream( stream );

    // DOS-PORT: build-time audio tier -- see musicPlayer.cpp's own
    // comment on DOSSAGE_SAMPLE_RATE/DOSSAGE_AUDIO_CHANNELS for the
    // full tradeoff. The stream/synthesis above always runs at full
    // stereo fidelity (synthesizeAudioCallback/Timbre/Envelope stay
    // completely untouched either way -- this only changes the shipped
    // asset's format, not the musical content generation); only the
    // final WAV write differs by tier.
#ifndef DOSSAGE_AUDIO_CHANNELS
#define DOSSAGE_AUDIO_CHANNELS 2
#endif

    FILE *outFile = fopen( outPath, "wb" );
    if( outFile == NULL ) {
        fprintf( stderr, "couldn't open %s for writing\n", outPath );
        return 1;
        }

#if DOSSAGE_AUDIO_CHANNELS == 1
    // Low tier: downmix to mono (average L+R, clamp-free by
    // construction -- both inputs are already in [-32768, 32767], and
    // (a+b)/2 of two same-range values stays in that range). A real
    // fidelity tradeoff, not a free win -- Passage's music genuinely has
    // independent per-note left/right loudness (mLoudnessLeft/Right,
    // decoded from music.tga's green/red channels), authored content,
    // not incidental panning; the pump-loop cost cut is the reason this
    // tier exists at all.
    std::vector<Uint8> monoOutput;
    monoOutput.reserve( output.size() / 2 );

    const Sint16 *stereoSamples = (const Sint16 *)output.data();
    int numStereoFrames = (int)( output.size() / 4 );

    for( int i=0; i<numStereoFrames; i++ ) {
        int left = stereoSamples[ i * 2 ];
        int right = stereoSamples[ i * 2 + 1 ];
        Sint16 monoSample = (Sint16)( ( left + right ) / 2 );

        monoOutput.insert( monoOutput.end(),
                           (Uint8 *)&monoSample, (Uint8 *)&monoSample + 2 );
        }

    writeWavHeader( outFile, (Uint32)monoOutput.size(), sampleRate, 1, 16 );
    fwrite( monoOutput.data(), 1, monoOutput.size(), outFile );
    fclose( outFile );

    printf( "Wrote %s (%zu bytes PCM + 44 byte header, mono, "
            "downmixed from %zu bytes stereo)\n",
            outPath, monoOutput.size(), output.size() );
#else
    // High tier: write the full-fidelity stereo output directly, no
    // downmix.
    writeWavHeader( outFile, (Uint32)output.size(), sampleRate, 2, 16 );
    fwrite( output.data(), 1, output.size(), outFile );
    fclose( outFile );

    printf( "Wrote %s (%zu bytes PCM + 44 byte header, stereo)\n",
            outPath, output.size() );
#endif

    return 0;
    }
