/*
 * GrandOrgue - free pipe organ simulator
 *
 * Copyright 2006 Milan Digital Audio LLC
 * Copyright 2009-2011 GrandOrgue contributors (see AUTHORS)
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
 * MA 02111-1307, USA.
 */

#ifndef GORGUEENCLOSURE_H_
#define GORGUEENCLOSURE_H_

#include "GOrgueMidiReceiver.h"
#include <wx/wx.h>

class GOrgueMidiEvent;
class GrandOrgueFile;
class IniFileConfig;

class GOrgueEnclosure
{
private:
	wxString m_group;
	GOrgueMidiReceiver m_midi;
	GrandOrgueFile* m_organfile;
	int m_AmpMinimumLevel;
	int m_MIDIInputNumber;
	int m_MIDIValue;
	wxString m_Name;
	bool m_Displayed;

public:

	GOrgueEnclosure(GrandOrgueFile* organfile);
	virtual ~GOrgueEnclosure();
	void Init(IniFileConfig& cfg, wxString group, wxString Name);
	void Load(IniFileConfig& cfg, wxString group, int enclosure_nb);
	void Save(IniFileConfig& cfg, bool prefix);
	void Set(int n);
	void ProcessMidi(const GOrgueMidiEvent& event);
	GOrgueMidiReceiver& GetMidiReceiver();
	const wxString& GetName();
	int GetValue();
	int GetMIDIInputNumber();
	float GetAttenuation();

	void Scroll(bool scroll_up);
	bool IsDisplayed();
};

#endif /* GORGUEENCLOSURE_H_ */
