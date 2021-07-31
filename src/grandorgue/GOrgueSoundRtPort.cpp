/*
 * GrandOrgue - free pipe organ simulator
 *
 * Copyright 2006 Milan Digital Audio LLC
 * Copyright 2009-2019 GrandOrgue contributors (see AUTHORS)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "GOrgueSoundRtPort.h"

#include <wx/log.h>
#include <wx/intl.h>

GOrgueSoundRtPort::GOrgueSoundRtPort(GOrgueSound* sound, RtAudio* rtApi, wxString name)
  : GOrgueSoundPort(sound, name),
    m_rtApi(rtApi),
    m_nBuffers(0)
{
}

GOrgueSoundRtPort::~GOrgueSoundRtPort()
{
	Close();
	try
	{
		if (m_rtApi) {
		  const RtAudio* rtApi = m_rtApi;
		  
		  m_rtApi = NULL;
		  delete rtApi;
		}
	}
	catch (RtAudioError &e)
	{
		wxString error = wxString::FromAscii(e.getMessage().c_str());
		wxLogError(_("RtAudio error: %s"), error.c_str());
	}
}

void GOrgueSoundRtPort::Open()
{
	Close();
	if (!m_rtApi)
		throw wxString::Format(_("Audio device %s not initialised"), m_Name.c_str());

	try
	{
		RtAudio::StreamParameters aOutputParam;
		aOutputParam.deviceId = -1;
		aOutputParam.nChannels = m_Channels;

		for (unsigned i = 0; i < m_rtApi->getDeviceCount(); i++)
			if (getName(m_rtApi, i) == m_Name)
				aOutputParam.deviceId = i;

		RtAudio::StreamOptions aOptions;
		aOptions.flags = RTAUDIO_MINIMIZE_LATENCY;
		aOptions.numberOfBuffers = (m_Latency * m_SampleRate) / (m_SamplesPerBuffer * 1000);
		aOptions.streamName = "GrandOrgue";

		unsigned samples_per_buffer = m_SamplesPerBuffer;
	

		m_rtApi->openStream(&aOutputParam, NULL, RTAUDIO_FLOAT32, m_SampleRate, &samples_per_buffer, &Callback, this, &aOptions);
		m_nBuffers = aOptions.numberOfBuffers;
		if (samples_per_buffer != m_SamplesPerBuffer)
		{
			if (samples_per_buffer != m_SamplesPerBuffer)
				throw wxString::Format(_("Device %s wants a different samples per buffer settings: %d.\nPlease adjust the GrandOrgue audio settings."), m_Name.c_str(), samples_per_buffer);
		}
		m_IsOpen = true;
	}
	catch (RtAudioError &e)
	{
		wxString error = wxString::FromAscii(e.getMessage().c_str());
		throw wxString::Format(_("RtAudio error: %s"), error.c_str());
	}
}

void GOrgueSoundRtPort::StartStream()
{
	if (!m_rtApi || !m_IsOpen)
		throw wxString::Format(_("Audio device %s not open"), m_Name.c_str());

	try
	{
		m_rtApi->startStream();
		double actual_latency = m_rtApi->getStreamLatency();

		/* getStreamLatency returns zero if not supported by the API, in which
		 * case we will make a best guess.
		 */
		if (actual_latency == 0)
			actual_latency = m_SamplesPerBuffer * m_nBuffers;

		SetActualLatency(actual_latency / m_SampleRate);
	}
	catch (RtAudioError &e)
	{
		wxString error = wxString::FromAscii(e.getMessage().c_str());
		throw wxString::Format(_("RtAudio error: %s"), error.c_str());
	}

	if (m_rtApi->getStreamSampleRate() != m_SampleRate)
		throw wxString::Format(_("Sample rate of device %s changed"), m_Name.c_str());
}

void GOrgueSoundRtPort::Close()
{
	if (!m_rtApi || !m_IsOpen)
		return;
	try
	{
		m_rtApi->abortStream();
	}
	catch (RtAudioError &e)
	{
		wxString error = wxString::FromAscii(e.getMessage().c_str());
		wxLogError(_("RtAudio error: %s"), error.c_str());
	}
	try
	{
		m_rtApi->closeStream();
	}
	catch (RtAudioError &e)
	{
		wxString error = wxString::FromAscii(e.getMessage().c_str());
		wxLogError(_("RtAudio error: %s"), error.c_str());
	}
	m_IsOpen = false;
}

int GOrgueSoundRtPort::Callback(void *outputBuffer, void *inputBuffer, unsigned int nFrames, double streamTime, RtAudioStreamStatus status, void *userData)
{
	GOrgueSoundRtPort* port = (GOrgueSoundRtPort*)userData;
	if (port->AudioCallback((float*)outputBuffer, nFrames))
		return 0;
	else
		return 1;
}

wxString GOrgueSoundRtPort::getName(RtAudio* rt_api, unsigned index)
{
  wxString apiName = RtAudio::getApiName(rt_api->getCurrentApi());
  wxString devName;

  try
  {
    RtAudio::DeviceInfo info = rt_api->getDeviceInfo(index);
    devName = wxString(info.name);
  }
  catch (RtAudioError &e)
  {
    wxString error = wxString::FromAscii(e.getMessage().c_str());
    wxLogError(_("RtAudio error: %s"), error.c_str());
    devName = wxString::Format(_("<unknown> %d"), index);
}
  return composeDeviceName(getSubsysName(), apiName, devName);
}

wxString get_oldstyle_name(RtAudio::Api api, RtAudio* rt_api, unsigned index)
{
  wxString apiName;

  switch (api)
  {
  case RtAudio::LINUX_ALSA:
	  apiName = wxT("Alsa");
	  break;
  case RtAudio::LINUX_OSS:
	  apiName = wxT("OSS");
	  break;
  case RtAudio::LINUX_PULSE:
	  apiName = wxT("PulseAudio");
	  break;
  case RtAudio::MACOSX_CORE:
	  apiName = wxT("Core");
	  break;
  case RtAudio::UNIX_JACK:
	  apiName = wxT("Jack");
	  break;
  case RtAudio::WINDOWS_ASIO:
	  apiName = wxT("ASIO");
	  break;
  case RtAudio::WINDOWS_DS:
	  apiName = wxT("DirectSound");
	  break;
  case RtAudio::WINDOWS_WASAPI:
	  apiName = wxT("WASAPI");
	  break;
  case RtAudio::UNSPECIFIED:
  default:
	  apiName = wxT("Unknown");
  }

  wxString prefix = apiName + wxT(": ");
  wxString devName;
  try
  {
	  RtAudio::DeviceInfo info = rt_api->getDeviceInfo(index);
	  devName = wxString(info.name);
  }
  catch (RtAudioError &e)
  {
	  wxString error = wxString::FromAscii(e.getMessage().c_str());
	  wxLogError(_("RtAudio error: %s"), error.c_str());
	  devName = wxString::Format(_("<unknown> %d"), index);
  }
  return apiName + wxT(": ") + devName;
}

GOrgueSoundPort* GOrgueSoundRtPort::create(GOrgueSound* sound, wxString name)
{
  GOrgueSoundRtPort* port = NULL;
  
  try
  {
    NameParser parser(name);
    const wxString subsysName = parser.nextComp();
    wxString apiName = subsysName == getSubsysName() ? parser.nextComp() : wxT("");
    
    std::vector<RtAudio::Api> rtaudio_apis;
    RtAudio::getCompiledApi(rtaudio_apis);

    for (unsigned k = 0; k < rtaudio_apis.size(); k++)
    {
      const RtAudio::Api apiIndex = rtaudio_apis[k];
      
      if (apiName == RtAudio::getApiName(apiIndex) || apiName.IsEmpty()) {
	RtAudio* audioApi = NULL;

	try
	{
	  audioApi = new RtAudio(apiIndex);
	  unsigned int deviceCount = audioApi->getDeviceCount();
	  
	  for (unsigned i = 0; i < deviceCount; i++)
	  {
	    const wxString devName = getName(audioApi, i);

	    if (
	      devName == name
	      || (apiName.IsEmpty() && get_oldstyle_name(apiIndex, audioApi, i) == name)
	    )
	    {
	      port = new GOrgueSoundRtPort(sound, audioApi, devName);
	      break;
	    }
	  }
	}
	catch (RtAudioError &e)
	{
	  wxString error = wxString::FromAscii(e.getMessage().c_str());
	  wxLogError(_("RtAudio error: %s"), error.c_str());
	}
	if (port)
	  break;
	// here audioApi is not used by port
	if (audioApi)
	  delete audioApi;
      }
    }
  }
  catch (RtAudioError &e)
  {
	  wxString error = wxString::FromAscii(e.getMessage().c_str());
	  wxLogError(_("RtAudio error: %s"), error.c_str());
  }
  return port;
}

void GOrgueSoundRtPort::addDevices(std::vector<GOrgueSoundDevInfo>& result)
{
	try
	{
		std::vector<RtAudio::Api> rtaudio_apis;
		RtAudio::getCompiledApi(rtaudio_apis);

		for (unsigned k = 0; k < rtaudio_apis.size(); k++)
		{
			RtAudio* audioDevice = 0;

			try
			{
				audioDevice = new RtAudio(rtaudio_apis[k]);
				for (unsigned i = 0; i < audioDevice->getDeviceCount(); i++)
				{
					RtAudio::DeviceInfo dev_info = audioDevice->getDeviceInfo(i);
					if (!dev_info.probed)
						continue;
					if (dev_info.outputChannels < 1)
						continue;
					GOrgueSoundDevInfo info;
					info.channels = dev_info.outputChannels;
					info.isDefault = dev_info.isDefaultOutput;
					info.name = getName(audioDevice, i);
					result.push_back(info);
				}
			}
			catch (RtAudioError &e)
			{
				wxString error = wxString::FromAscii(e.getMessage().c_str());
				wxLogError(_("RtAudio error: %s"), error.c_str());
			}
			if (audioDevice)
				delete audioDevice;
		}
	}
	catch (RtAudioError &e)
	{
		wxString error = wxString::FromAscii(e.getMessage().c_str());
		wxLogError(_("RtAudio error: %s"), error.c_str());
	}
}
