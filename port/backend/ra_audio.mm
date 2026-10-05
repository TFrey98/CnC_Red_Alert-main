/*
**	ra_audio.mm -- audio output: one CoreAudio output unit playing interleaved
**	stereo float samples that the engine side mixes (see ra_platform.h).
**
**	Built WITHOUT -DWIN32 and WITHOUT -include wwcompat.h (see ra_platform.h).
*/
#import <AudioToolbox/AudioToolbox.h>
#include "ra_platform.h"

static AudioComponentInstance Unit = NULL;
static RA_Audio_Render Render = NULL;
static void * RenderUser = NULL;

static OSStatus render_callback(void * ref, AudioUnitRenderActionFlags * flags, const AudioTimeStamp * when,
	UInt32 bus, UInt32 frames, AudioBufferList * data)
{
	(void)ref; (void)flags; (void)when; (void)bus;
	float * out = (float *)data->mBuffers[0].mData;
	if (Render) Render(out, (int)frames, RenderUser);
	else memset(out, 0, frames * 2 * sizeof(float));
	return noErr;
}

int RA_Audio_Start(RA_Audio_Render render, void * user, int * rate)
{
	if (Unit != NULL) RA_Audio_Stop();
	AudioComponentDescription desc = {kAudioUnitType_Output, kAudioUnitSubType_DefaultOutput,
		kAudioUnitManufacturer_Apple, 0, 0};
	AudioComponent comp = AudioComponentFindNext(NULL, &desc);
	if (comp == NULL || AudioComponentInstanceNew(comp, &Unit) != noErr) {Unit = NULL; return 0;}

	/* Run at the device's own rate, so CoreAudio does no second resampling. */
	AudioStreamBasicDescription hw;
	UInt32 size = sizeof(hw);
	double sr = 48000;
	if (AudioUnitGetProperty(Unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &hw, &size) == noErr
	    && hw.mSampleRate > 0) sr = hw.mSampleRate;

	AudioStreamBasicDescription fmt = {0};
	fmt.mSampleRate = sr;
	fmt.mFormatID = kAudioFormatLinearPCM;
	fmt.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
	fmt.mChannelsPerFrame = 2;
	fmt.mBitsPerChannel = 32;
	fmt.mFramesPerPacket = 1;
	fmt.mBytesPerFrame = 8;
	fmt.mBytesPerPacket = 8;
	Render = render;
	RenderUser = user;
	AURenderCallbackStruct cb = {render_callback, NULL};
	if (AudioUnitSetProperty(Unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &fmt, sizeof(fmt)) != noErr
	    || AudioUnitSetProperty(Unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &cb, sizeof(cb)) != noErr
	    || AudioUnitInitialize(Unit) != noErr || AudioOutputUnitStart(Unit) != noErr) {
		AudioComponentInstanceDispose(Unit);
		Unit = NULL;
		return 0;
	}
	if (rate) *rate = (int)sr;
	return 1;
}

void RA_Audio_Stop(void)
{
	if (Unit == NULL) return;
	AudioOutputUnitStop(Unit);
	AudioUnitUninitialize(Unit);
	AudioComponentInstanceDispose(Unit);
	Unit = NULL;
}
