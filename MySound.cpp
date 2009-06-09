/*
 * GrandOrgue - free pipe organ simulator based on MyOrgan
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 */

#include "MyOrgan.h"

struct_WAVE WAVE = {{'R','I','F','F'}, 0, {'W','A','V','E'}, {'f','m','t',' '}, 16, 3, 2, 44100, 352800, 8, 32, {'d','a','t','a'}, 0};

extern MyOrganFile* organfile;
extern const char* s_MIDIMessages[];
extern long i_MIDIMessages[];
MySound* g_sound = 0;

#ifdef _WIN32
 RtAudio::RtAudioApi i_APIs[] = { RtAudio::WINDOWS_DS, RtAudio::WINDOWS_ASIO };
 char* s_APIs[] = { "DirectSound: ", "ASIO: " };
#endif

#ifdef linux
  RtAudio::RtAudioApi i_APIs[] = {RtAudio::LINUX_JACK, RtAudio::LINUX_ALSA};// RtAudio::LINUX_OSS };
  char* s_APIs[] = {"Jack: ", "Alsa: " }; //{ " Jack: ", "Alsa: ", "OSS: " };
#endif

#include "MySoundCallbackMIDI.h"
#include "MySoundCallbackAudio.h"

DEFINE_EVENT_TYPE(wxEVT_DRAWSTOP)
DEFINE_EVENT_TYPE(wxEVT_PUSHBUTTON)
DEFINE_EVENT_TYPE(wxEVT_ENCLOSURE)
DEFINE_EVENT_TYPE(wxEVT_NOTEONOFF)
DEFINE_EVENT_TYPE(wxEVT_LISTENING)
DEFINE_EVENT_TYPE(wxEVT_METERS)
DEFINE_EVENT_TYPE(wxEVT_LOADFILE)

MySound::MySound(void)
{
	int i, j, k;

	m_parent = 0;

	b_active = b_listening = false;
	g_sound = this;
	pConfig = wxConfigBase::Get();

	midiDevices = 0;
	b_midiDevices = 0;
	i_midiDevices = 0;
	audioDevice = 0;

	meter_counter = meter_poly = 0;
	meter_left = meter_right = 0.0;
	b_stoprecording = b_memset = false;
	f_output = 0;

	try
	{
		for (k = 0; k < (int)(sizeof(i_APIs) / sizeof(RtAudio::RtAudioApi)); k++)
		{
        	    audioDevice = new RtAudio(i_APIs[k]);

			for (i = 1; i <= audioDevice->getDeviceCount(); i++)
			{
				RtAudioDeviceInfo info = audioDevice->getDeviceInfo(i);
				wxString name = info.name.c_str();
				name.Replace("\\", "|");
				name = s_APIs[k] + name;
				if (info.isDefault && defaultAudio.IsEmpty())
					defaultAudio = name;
				if (i_APIs[k] != RtAudio::WINDOWS_DS)
				{
					for (j = 0; j < (int)info.sampleRates.size(); j++)
						if (info.sampleRates[j] == 44100)
							break;
				}
				else
				{
					j = 0;
					if (info.sampleRates.size() && info.sampleRates.back() < 44100)
						j = info.sampleRates.size();
				}
				if (info.outputChannels < 2 || !info.probed || j == (int)info.sampleRates.size() || m_audioDevices.find(name) != m_audioDevices.end())
					continue;
				m_audioDevices[name] = std::make_pair(i, i_APIs[k]);
			}
			delete audioDevice;
			audioDevice = 0;
		}

	        RtMidiIn midiDevice;
		n_midiDevices = midiDevice.getPortCount();

		midiDevices = new RtMidiIn*[n_midiDevices];
		b_midiDevices = new bool[n_midiDevices];
		i_midiDevices = new int[n_midiDevices];
		for (i = 0; i < n_midiDevices; i++)
		{
			midiDevices[i] = new RtMidiIn();
			wxString name = midiDevices[i]->getPortName(i).c_str();
			name.Replace("\\", "|");
			m_midiDevices[name] = i;
			b_midiDevices[i] = false;
			i_midiDevices[i] = 0;
		}
	}
	catch (RtError &e)
	{
		e.printMessage();
		CloseSound();
	}
}

MySound::~MySound(void)
{
	CloseSound();
	try
	{
		if (midiDevices)
		{
			for (int i = 0; i < n_midiDevices; i++)
			{
				if (midiDevices[i])
				{
					if (b_midiDevices[i])
						midiDevices[i]->closePort();
					delete midiDevices[i];
				}
			}
			delete [] midiDevices;
			midiDevices = 0;
		}
		if (b_midiDevices)
			delete b_midiDevices;
		if (i_midiDevices)
			delete i_midiDevices;
		if (audioDevice)
		{
			delete audioDevice;
			audioDevice = 0;
		}
	}
	catch (RtError &e)
	{
		e.printMessage();
	}
}

bool MySound::OpenSound(bool wait)
{
	int i;

	std::map<wxString, int>::iterator it2;
	for (it2 = m_midiDevices.begin(); it2 != m_midiDevices.end(); it2++)
		pConfig->Read("Devices/MIDI/" + it2->first, -1);
	for (i = 0; i < 16; i++)
		i_midiEvents[i] = pConfig->Read(wxString("MIDI/") + s_MIDIMessages[i], i_MIDIMessages[i]);

	defaultAudio = pConfig->Read("Devices/DefaultSound", defaultAudio);
	n_latency = pConfig->Read("Devices/Sound/" + defaultAudio, 12);
	volume = pConfig->Read("Volume", 50);
	polyphony = pConfig->Read("PolyphonyLimit", 1024);
	poly_soft = (polyphony * 3) / 4;
	b_stereo = pConfig->Read("StereoEnabled", 1);
	b_limit  = pConfig->Read("ManagePolyphony", 1);
	b_align  = pConfig->Read("AlignRelease", 1);
	b_detach = pConfig->Read("DetachRelease", 1);
	b_scale  = pConfig->Read("ScaleRelease", 1);
	b_random = pConfig->Read("RandomizeSpeaking", 1);

	samplers_count = 0;
	for (i = 0; i < MAX_POLYPHONY; i++)
		samplers_open[i] = samplers + i;
	for (i = 0; i < 26; i++)
		windchests[i] = 0;

	try
	{
		std::map<wxString, int>::iterator it2;
		int n_midi = 0;
		for (it2 = m_midiDevices.begin(); it2 != m_midiDevices.end(); it2++)
		{
			i = pConfig->Read("Devices/MIDI/" + it2->first, 0L);
			if (i >= 0)
			{
				n_midi++;
				i_midiDevices[it2->second] = i;
				if (!b_midiDevices[it2->second])
				{
					b_midiDevices[it2->second] = true;
 					midiDevices[it2->second]->openPort(it2->second);
 					//midiDevices[it2->second]->ignoreTypes(false,false,false);
				}
			}
			else if (b_midiDevices[it2->second])
			{
				b_midiDevices[it2->second] = false;
				midiDevices[it2->second]->closePort();
			}
		}

		std::map<wxString, std::pair<int, RtAudio::RtAudioApi> >::iterator it;
		it = m_audioDevices.find(defaultAudio.c_str());
		if (it != m_audioDevices.end())
		{
			int bufferSize, numberOfBuffers;
			audioDevice = new RtAudio(it->second.second);
			if (defaultAudio.StartsWith("ASIO: "))
			{
			    numberOfBuffers = 2;
			    bufferSize = (n_latency * 441 + numberOfBuffers * 5) / (numberOfBuffers * 10);
			    bufferSize &= 0xFFFFFFF0; // i think it only needs to be aligned to x2, but just to be sane and safe
			    if (bufferSize < 64) bufferSize = 64;
		            if (bufferSize > 1024) bufferSize = 1024;
			}
			else if (defaultAudio.StartsWith("Jack: "))
			{
			    numberOfBuffers = 1;
			    bufferSize=1024;
			}
			else
			{
			    bufferSize = 128;
			    numberOfBuffers = (n_latency * 441 + bufferSize * 5) / (bufferSize * 10);
			    if (numberOfBuffers < 2) numberOfBuffers = 2;
			}

			audioDevice->openStream(it->second.first, 2, 0, 0, RTAUDIO_FLOAT32, 44100, &bufferSize, numberOfBuffers);

			if (bufferSize <= 1024)
			{
				n_latency = (bufferSize * numberOfBuffers * 10 + 221) / 441;
				pConfig->Write("Devices/Sound/" + defaultAudio, n_latency);
				audioDevice->setStreamCallback(&MySoundCallbackAudio, 0);
				audioDevice->startStream();
			}
			else
			{
		                ::wxSleep(1);
		                if (m_parent)
		                    ::wxLogError("Cannot use buffer size above 1024 samples; unacceptable quantization would occur.");
		                CloseSound();
		                return false;
			}
			RtAudioDeviceInfo info = audioDevice->getDeviceInfo(it->second.first);
			format = audioDevice->getOutputFormat();
		}

		if (!n_midi || it == m_audioDevices.end())
		{
            ::wxSleep(1);
			if (m_parent)
                ::wxLogWarning(n_midi ? "No audio device is selected; neither MIDI input nor sound output will occur!" : "No MIDI devices are selected for listening; neither MIDI input nor sound output will occur!");
			CloseSound();
			return false;
		}
	}
	catch (RtError &e)
	{
		::wxSleep(1);
		if (m_parent)
            e.printMessage();
		CloseSound();
		return false;
	}

	if (wait)
		::wxSleep(1);

	return true;
}

void MySound::CloseSound()
{
	bool was_active = b_active;

	if (f_output)
	{
		b_stoprecording = true;
		::wxMilliSleep(100);
		CloseWAV();		// this should never be necessary...
	}

	b_active = false;

	try
	{
		if (audioDevice)
		{
			audioDevice->abortStream();
			audioDevice->closeStream();
			delete audioDevice;
			audioDevice = 0;
		}
	}
	catch (RtError &e)
	{
		e.printMessage();
	}

	::wxMilliSleep(10);
	if (was_active)
		MIDIAllNotesOff();
}

bool MySound::ResetSound()
{
	wxBusyCursor busy;
	bool was_active = b_active;

	int temp = g_sound->volume;
	CloseSound();
	if (!OpenSound())
		return false;
	if (!temp)  // don't let resetting sound reactivate an organ
        g_sound->volume = temp;
	b_active = was_active;
	if (organfile)
	{
        OrganDocument* doc = (OrganDocument*)(::wxGetApp().m_docManager->GetCurrentDocument());
        if (doc && doc->b_loaded)
        {
            b_active = true;
            for (int i = 0; i < organfile->NumberOfTremulants; i++)
            {
                if (organfile->tremulant[i].DefaultToEngaged)
                {
                    organfile->tremulant[i].Set(false);
                    organfile->tremulant[i].Set(true);
                }
            }
        }
	}
	return true;
}

void MySound::OpenWAV()
{
	if (f_output)
        return;
	wxFileDialog dlg(::wxGetApp().frame, _("Save as"), wxConfig::Get()->Read("wavPath", ::wxGetApp().m_path), wxEmptyString, _("WAV files (*.wav)|*.wav"), wxSAVE | wxOVERWRITE_PROMPT);
	if (dlg.ShowModal() == wxID_OK)
	{
		wxConfig::Get()->Write("wavPath", dlg.GetDirectory());
		b_stoprecording = false;
		if (f_output = fopen(dlg.GetPath(), "wb"))
			fwrite(&WAVE, sizeof(WAVE), 1, f_output);
		else
			::wxLogError("Unable to open file for writing");
	}
}

void MySound::CloseWAV()
{
	if (!f_output)
		return;
	WAVE.Subchunk2Size = (WAVE.ChunkSize = ftell(f_output) - 8) - 36;
    fseek(f_output, 0, SEEK_SET);
	fwrite(&WAVE, sizeof(WAVE), 1, f_output);
	fclose(f_output);
	b_stoprecording = false;
	f_output = 0;
}

void MySound::UpdateOrganMIDI()
{
    long count=pConfig->Read("OrganMIDI/Count", 0L);
    for (long i=0; i<count; i++) {
        wxString itemstr="OrganMIDI/Organ"+wxString::Format("%d", i);
        long j=pConfig->Read(itemstr+".midi", 0L);
        wxString file=pConfig->Read(itemstr+".file");
        organmidiEvents.insert(std::pair<long, wxString>(j, file) );
    }
}
