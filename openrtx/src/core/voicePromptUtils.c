/***************************************************************************
 *   Copyright (C) 2022 by Federico Amedeo Izzo IU2NUO,                    *
 *                         Niccolò Izzo IU2KIN,                            *
 *                         Silvano Seva IU2KWO                             *
 *                         Joseph Stephen VK7JS                            *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/
 // This file contains functions for announcing radio functions using the building blocks in voicePrompts.h/c.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <state.h>
#include "interfaces/cps_io.h"
#include "core/voicePromptUtils.h"

static void vpInitIfNeeded(VoicePromptQueueFlags_T flags)
{
	if (flags & vpqInit)
		vpInit();
}

static void vpPlayIfNeeded(VoicePromptQueueFlags_T flags)
{
	uint8_t vpLevel = state.settings.vpLevel;
	
	if ((flags & vpqPlayImmediately)
			|| ((flags & vpqPlayImmediatelyAtMediumOrHigher) && (vpLevel >= vpMedium)))
		vpPlay();
}

static void removeUnnecessaryZerosFromVoicePrompts(char *str)
{
	const int NUM_DECIMAL_PLACES = 1;
	int len = strlen(str);
	for(int i = len; i > 2; i--)
	{
		if ((str[i - 1] != '0') || (str[i - (NUM_DECIMAL_PLACES + 1)] == '.'))
		{
			str[i] = 0;
			return;
		}
	}
}
void announceVFO()
{
	vpInit();
	
	vpQueuePrompt(PROMPT_VFO);
	
	vpPlay();	
}

void announceChannelName(channel_t* channel, uint16_t channelIndex, 
VoicePromptQueueFlags_T flags)
{
	vpInitIfNeeded(flags);
	
	if (flags & vpqIncludeDescriptions)
	{
		vpQueuePrompt(PROMPT_CHANNEL);
	}
	vpQueueInteger(channelIndex);
	
	// Only queue the name if it is not the same as the raw number.
	// Otherwise the radio will say channel 1 1 for channel 1.
	char numAsStr[16]="\0";
    snprintf(numAsStr, 16, "%d", channelIndex);
	if (strcmp(numAsStr, channel->name) != 0)
		vpQueueString(channel->name, vpAnnounceCommonSymbols);
	
	vpPlayIfNeeded(flags);
}

void vpQueueFrequency(freq_t freq)
{
		char buffer[16];
	int mhz = (freq / 1000000);
	int khz = ((freq%1000000) / 10);
	
	snprintf(buffer, 16, "%d.%05d", mhz, khz);
	removeUnnecessaryZerosFromVoicePrompts(buffer);
	
	vpQueueString(buffer, vpAnnounceCommonSymbols);

	vpQueuePrompt(PROMPT_MEGAHERTZ);
}

void announceFrequencies(freq_t rx, freq_t tx, VoicePromptQueueFlags_T flags)
{
	vpInitIfNeeded(flags);
		// if rx and tx frequencies differ, announce both, otherwise just announce 
		// one.
	if (rx == tx)
		vpQueueFrequency(rx);
	else
	{
		vpQueuePrompt(PROMPT_RECEIVE);
		vpQueueFrequency(rx);
		vpQueuePrompt(PROMPT_TRANSMIT);
		vpQueueFrequency(tx);
	}
	vpPlayIfNeeded(flags);
} 

void announceRadioMode(uint8_t mode, VoicePromptQueueFlags_T flags)
{
	vpInitIfNeeded(flags);

	if (flags & vpqIncludeDescriptions)
		vpQueuePrompt(PROMPT_MODE);
	
	switch(mode)
	{
		case OPMODE_DMR:
			vpQueueStringTableEntry(&currentLanguage->dmr);
			break;
		case OPMODE_FM:
			vpQueueStringTableEntry(&currentLanguage->fm);
			break;
		case OPMODE_M17:
			vpQueueStringTableEntry(&currentLanguage->m17);
			break;
	}
	
	vpPlayIfNeeded(flags);
}

void announceBandwidth(uint8_t bandwidth, VoicePromptQueueFlags_T flags)
{
		if (bandwidth > BW_25)
		bandwidth = BW_25; // should probably never happen!

	vpInitIfNeeded(flags);
	
		if (flags & vpqIncludeDescriptions)
		vpQueuePrompt(PROMPT_BANDWIDTH);
	
	char* bandwidths[]={"12.5", "20", "25"};
	vpQueueString(bandwidths[bandwidth], vpAnnounceCommonSymbols);
	vpQueuePrompt(PROMPT_KILOHERTZ);
	
	vpPlayIfNeeded(flags);
}

void anouncePower(float power, VoicePromptQueueFlags_T flags)
{
		vpInitIfNeeded(flags);
		
	char buffer[16] = "\0";
	
	if (flags & vpqIncludeDescriptions)
		vpQueuePrompt(PROMPT_POWER);
	
	snprintf(buffer, 16, "%1.1f", power);
	vpQueueString(buffer, vpAnnounceCommonSymbols);
	vpQueuePrompt(PROMPT_WATTS);
	
	vpPlayIfNeeded(flags);
}

void announceChannelSummary(channel_t* channel, uint16_t channelIndex, 
VoicePromptQueueFlags_T flags)
{
	 	if (!channel) return;
		
	vpInitIfNeeded(flags);
	
	// mask off init and play because this function will handle init and play.
	VoicePromptQueueFlags_T localFlags=flags & ~(vpqInit | vpqPlayImmediately); 
	// Force on the descriptions for level 3.
	if (state.settings.vpLevel == vpHigh)
		localFlags |= vpqIncludeDescriptions;
	// if VFO mode, announce VFO.
	// channelIndex will be 0 if called from VFO mode.
	if (channelIndex == 0)
		vpQueuePrompt(PROMPT_VFO);
	else
		announceChannelName(channel, channelIndex, localFlags);
	announceFrequencies(channel->rx_frequency , channel->tx_frequency, localFlags);
	announceRadioMode(channel->mode,  localFlags);
	
	if (channel->mode == OPMODE_FM)
	{
		announceBandwidth(channel->bandwidth, localFlags);
		
		if (channel->fm.rxToneEn || channel->fm.txToneEn)
		{
			announceCTCSS(channel->fm.rxToneEn, channel->fm.rxTone, 
	channel->fm.txToneEn, channel->fm.txTone, 
	localFlags);
		}
	}
	else if (channel->mode == OPMODE_M17)
	{
		announceContactWithIndex(channel->m17.contact_index, localFlags);
	}
	else if (channel->mode == OPMODE_DMR)
	{
		announceContactWithIndex(channel->dmr.contact_index, localFlags);
		// Force announcement of the words timeslot and colorcode to avoid ambiguity. 
		announceTimeslot(channel->dmr.dmr_timeslot, (localFlags | vpqIncludeDescriptions));
		announceColorCode(channel->dmr.rxColorCode, channel->dmr.txColorCode, (localFlags | vpqIncludeDescriptions));
	}

	anouncePower(channel->power, localFlags);
	
	vpPlayIfNeeded(flags);
}
 
void AnnounceInputChar(char ch)
{
	char buf[2] = "\0";
	buf[0] = ch;
	
		vpInit();
		
	uint8_t flags = vpAnnounceCaps | vpAnnounceSpace | vpAnnounceCommonSymbols | vpAnnounceLessCommonSymbols;
		
	vpQueueString(buf, flags);
	
	vpPlay();
}

void announceInputReceiveOrTransmit(bool tx, VoicePromptQueueFlags_T flags)
{
	vpInitIfNeeded(flags);
	
	if (tx)
		vpQueuePrompt(PROMPT_TRANSMIT);
	else
		vpQueuePrompt(PROMPT_RECEIVE);
		
	vpPlayIfNeeded(flags);
}

void ReplayLastPrompt()
{
	if (vpIsPlaying())
		vpTerminate();
	else
		vpPlay();
}

void announceError(VoicePromptQueueFlags_T flags)
{
	vpInitIfNeeded(flags);
		
	vpQueueStringTableEntry(&currentLanguage->error);
	
	vpPlayIfNeeded(flags);
}	

void announceText( char* text, VoicePromptQueueFlags_T flags)
{
		if (!text || !*text)
		return;
	
	vpInitIfNeeded(flags);
	// see if we have a prompt for this string.
	int offset = GetEnglishStringTableOffset(text);
	
	if (offset != -1)
		vpQueueStringTableEntry((const char* const *)(&currentLanguage->languageName + offset));
	else // just spel it out
		vpQueueString(text, (flags&~(vpqInit|vpqPlayImmediately)));
		
	vpPlayIfNeeded(flags);
}

void announceCTCSS(bool rxToneEnabled, uint8_t rxTone, bool txToneEnabled, uint8_t txTone, VoicePromptQueueFlags_T flags)
{
	vpInitIfNeeded(flags);
	
	if (!rxToneEnabled && !txToneEnabled)
	{
		vpQueuePrompt(PROMPT_TONE);
		vpQueueStringTableEntry(&currentLanguage->off);
		vpPlayIfNeeded(flags);
		return;
	}
	
	char buffer[16] = "\0";

	// If the rx and tx tones are the same and both are enabled, just say Tone.
	if ((rxToneEnabled && txToneEnabled) && (rxTone == txTone))
	{
		vpQueuePrompt(PROMPT_TONE);
		snprintf(buffer, 16, "%3.1f", ctcss_tone[rxTone]/10.0f);
		vpQueueString(buffer, vpqDefault);
		vpQueuePrompt(PROMPT_HERTZ);
		vpPlayIfNeeded(flags);
		return;
	}
	// speak the individual rx and tx tones.
	if (rxToneEnabled)
	{
		vpQueuePrompt(PROMPT_RECEIVE);
		vpQueuePrompt(PROMPT_TONE);
		snprintf(buffer, 16, "%3.1f", ctcss_tone[rxTone]/10.0f);
		vpQueueString(buffer, vpqDefault);
		vpQueuePrompt(PROMPT_HERTZ);
	}
	if (txToneEnabled)
	{
		vpQueuePrompt(PROMPT_TRANSMIT);
		vpQueuePrompt(PROMPT_TONE);
		snprintf(buffer, 16, "%3.1f", ctcss_tone[txTone]/10.0f);
		vpQueueString(buffer, vpqDefault);
		vpQueuePrompt(PROMPT_HERTZ);
	}
	
	vpPlayIfNeeded(flags);
}

void announceBrightness(uint8_t brightness, VoicePromptQueueFlags_T flags)
{
	vpInitIfNeeded(flags);
	
	if (flags & vpqIncludeDescriptions)
		vpQueueStringTableEntry(&currentLanguage->brightness);
		
	vpQueueInteger(brightness);
	
	vpPlayIfNeeded(flags);
}

void announceSquelch(uint8_t squelch, VoicePromptQueueFlags_T flags)
{
	vpInitIfNeeded(flags);
	
	if (flags & vpqIncludeDescriptions)
		vpQueuePrompt(PROMPT_SQUELCH);
		
	vpQueueInteger(squelch);
	
	vpPlayIfNeeded(flags);
}

void announceContact(contact_t* contact, VoicePromptQueueFlags_T flags)
{
	if (!contact)
		return;
	
	vpInitIfNeeded(flags);
	
	if (flags & vpqIncludeDescriptions)
		vpQueuePrompt(PROMPT_CONTACT);
	
	if (contact->name[0])
		vpQueueString(contact->name, vpAnnounceCommonSymbols);

	vpPlayIfNeeded(flags);
}

void announceContactWithIndex(uint16_t index, VoicePromptQueueFlags_T flags)
{
	if (index == 0)
		return;
	
	contact_t contact;
	
	if (cps_readContact(&contact, index) == -1)
		return;
	
	announceContact(&contact, flags);
}

void announceTimeslot(uint8_t timeslot, VoicePromptQueueFlags_T flags)
{
	vpInitIfNeeded(flags);
	
	if (flags & vpqIncludeDescriptions)
		vpQueuePrompt(PROMPT_TIMESLOT);
	
	vpQueueInteger(timeslot);
	
	vpPlayIfNeeded(flags);
}

void  announceColorCode(uint8_t rxColorCode, uint8_t txColorCode, VoicePromptQueueFlags_T flags)
{
	vpInitIfNeeded(flags);
	
	if (flags & vpqIncludeDescriptions)
		vpQueuePrompt(PROMPT_COLORCODE);
	
	if (rxColorCode == txColorCode)
	{
		vpQueueInteger(rxColorCode);
	}
	else
	{
		vpQueuePrompt(PROMPT_RECEIVE);
		vpQueueInteger(rxColorCode);
		vpQueuePrompt(PROMPT_TRANSMIT);
		vpQueueInteger(txColorCode);
	}
	
	vpPlayIfNeeded(flags);
}
/*
there are 5 levels of verbosity:

vpNone: no voice or beeps.
vpBeep: beeps only.
vpLow: menus talk, but channel and frequency changes are indicated with a beep 
and only voiced on demand with f1.
vpMedium: menus, channel and frequency changes talk but with extra 
descriptions eliminated unless ambiguity would result. E.g. We'd say "FM" 
rather than "Mode: FM" in the channel summary.
vpHigh: like vpMedium except with extra descriptions: e.g. "Mode fm".
Also, if a voice prompt is in progress, e.g. changing a menu item, the descriptions are eliminated.
e.g. changing ctcss tones would not repeat "ctcss" when arrowing through the 
options rapidly.
*/
VoicePromptQueueFlags_T GetQueueFlagsForVoiceLevel()
{
	VoicePromptQueueFlags_T flags = vpqInit;
	
	uint8_t vpLevel = state.settings.vpLevel;
	switch (vpLevel)
	{
	case vpNone:
	case vpBeep:
		return vpqDefault;
	// play some immediately, other things on demand.
	case vpLow:
		flags |= vpqPlayImmediatelyAtMediumOrHigher;
		break;
	// play all immediatley but without extra descriptions
	case vpMedium:
	{
		flags |= vpqPlayImmediately;
		break;
	}
	// play immediatley with descriptions unless speech is in progress.
	case vpHigh:
		flags |= vpqPlayImmediately;
		if (!vpIsPlaying())
			flags |= vpqIncludeDescriptions;
		break;
	}
	
	return flags;
}